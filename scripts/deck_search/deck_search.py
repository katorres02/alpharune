#!/usr/bin/env python3
"""Search for a better version of a deck against one fixed opponent.

Starts from a deck list, tries swapping one card at a time (one copy out, one
copy in), simulates each variant against the opponent with the engine, and
keeps a variant only when it wins clearly more often than the current list.
It runs until its time budget ends and can be stopped and resumed at any time.

    python3 scripts/deck_search/deck_search.py \\
        --deck decks/draven_nuevo.txt \\
        --opponent decks/riven_radiance_namdang.txt \\
        --hours 7

Results go to --out (default: deck_search_runs/<deck>_vs_<opponent>/):
    report.md       readable summary (best list, win rates, accepted swaps)
    best_deck.txt   the best list found so far
    history.jsonl   every variant evaluated
    state.json      what --resume continues from

How the search saves time:
  * Candidate cards in priority order: the deck's sideboard, then cards from
    the other deck lists in decks/ that the legend can play, then every other
    legal card.
  * Successive halving: each round tries many swaps with few games, keeps the
    best quarter, gives those more games, and so on. Only the round's winner
    gets a long run against the current list.
  * The round's winner is then measured again with --confirm-games fresh
    games (the games that selected it are biased upwards), and accepted only
    if its win rate is higher than the current list's with a one-sided
    confidence of about 95%, twice: on that batch and again after a second
    fresh batch (many rounds at 95% would otherwise let lucky swaps in).

The agents are the engine's MCTS bots (they see the opponent's hand and play
below a good human), so a better list here is a strong hint, not a proof.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import random
import re
import subprocess
import sys
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / "build" / "riftbound"

SECTION_ORDER = ["legend", "champion", "maindeck", "battlefields", "runes", "sideboard"]
SECTION_TITLES = {"legend": "Legend", "champion": "Champion", "maindeck": "MainDeck",
                  "battlefields": "Battlefields", "runes": "Runes", "sideboard": "Sideboard"}


# ── Deck lists ───────────────────────────────────────────────────────────────
def parse_deck(text: str) -> dict[str, Counter]:
    deck = {s: Counter() for s in SECTION_ORDER}
    extra: list[str] = []          # sections we don't edit (e.g. Neeko Legends)
    sec = "maindeck"
    keep = True
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key = line.rstrip(":").replace(" ", "").lower()
        if line.endswith(":"):
            if key in deck:
                sec, keep = key, True
            else:
                sec, keep = None, False
                extra.append(line)
            continue
        if not keep:
            extra.append(line)
            continue
        m = re.match(r"^(\d+)\s*x?\s+(.+)$", line)
        if m and sec:
            deck[sec][m.group(2).strip()] += int(m.group(1))
    deck["_extra"] = extra  # type: ignore[assignment]
    return deck


def render_deck(deck: dict) -> str:
    out = []
    for s in SECTION_ORDER:
        if not deck[s]:
            continue
        out.append(f"{SECTION_TITLES[s]}:")
        for name, q in sorted(deck[s].items()):
            out.append(f"{q} {name}")
        out.append("")
    out.extend(deck.get("_extra", []))
    return "\n".join(out).rstrip() + "\n"


def copy_deck(deck: dict) -> dict:
    d = {s: Counter(deck[s]) for s in SECTION_ORDER}
    d["_extra"] = list(deck.get("_extra", []))
    return d


def deck_key(deck: dict) -> str:
    return ";".join(f"{q} {n}" for n, q in sorted(deck["maindeck"].items()))


# ── Card pool ────────────────────────────────────────────────────────────────
def load_registry(cache: Path) -> list[dict]:
    if not cache.exists() or cache.stat().st_mtime < BINARY.stat().st_mtime:
        subprocess.run([str(BINARY), "--dump-registry", str(cache)], cwd=ROOT, check=True,
                       capture_output=True, env={**os.environ, "RIFTBOUND_ROOT": str(ROOT)})
    data = json.loads(cache.read_text())
    cards = data.get("cards", data) if isinstance(data, dict) else data
    return list(cards.values()) if isinstance(cards, dict) else list(cards)


def find_legend(cards: list[dict], legend_line: str) -> dict:
    """Legends are written 'Champion, Title' in deck lists."""
    want = legend_line.lower()
    for c in cards:
        if c.get("card_type") != "legend":
            continue
        names = {c["name"].lower()} | {f"{t}, {c['name']}".lower() for t in c.get("tags") or []}
        if want in names:
            return c
    sys.exit(f"Legend not found in the engine: {legend_line}")


def legal_pool(cards: list[dict], legend: dict) -> set[str]:
    doms = set(legend.get("domains") or [])
    champ_tags = set(legend.get("tags") or [])
    pool = set()
    for c in cards:
        if c.get("card_type") not in ("unit", "spell", "gear"):
            continue
        if c.get("super_type") == "token" or c.get("banned"):
            continue
        if not set(c.get("domains") or []) <= doms:
            continue
        if c.get("super_type") == "signature" and not (set(c.get("tags") or []) & champ_tags):
            continue
        pool.add(c["name"])
    return pool


def candidate_order(deck: dict, pool: set[str], rng: random.Random) -> list[str]:
    """Sideboard first, then cards seen in other repo decks, then the rest."""
    first = [n for n in deck["sideboard"] if n in pool]
    seen = Counter()
    for p in sorted((ROOT / "decks").glob("*.txt")):
        d = parse_deck(p.read_text(encoding="utf-8"))
        for n, q in (d["maindeck"] + d["sideboard"]).items():
            if n in pool:
                seen[n] += q
    second = [n for n, _ in seen.most_common() if n not in first]
    rest = sorted(pool - set(first) - set(second))
    rng.shuffle(rest)
    return first + second + rest


# ── Simulation ───────────────────────────────────────────────────────────────
class Simulator:
    """Runs games on every core: each evaluation is split into small engine
    runs (one game seat each) executed in parallel."""

    def __init__(self, opponent: Path, agent: str, workdir: Path, timeout: int,
                 workers: int, chunk: int = 4):
        self.opponent = opponent
        self.agent = agent
        self.workdir = workdir
        self.timeout = timeout
        self.workers = workers
        self.chunk = chunk
        self.games_played = 0

    def _run(self, job: tuple[Path, Path, int, bool]) -> int:
        """Wins of the evaluated deck in one engine run."""
        d1, d2, games, deck_is_p1 = job
        cmd = [str(BINARY), "--deck1", str(d1), "--deck2", str(d2), "--games", str(games),
               "--threads", "1", "--agent1", self.agent, "--agent2", self.agent]
        out = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=self.timeout,
                             env={**os.environ, "RIFTBOUND_ROOT": str(ROOT)}).stdout
        p1 = re.search(r"P1 wins:\s+(\d+)", out)
        p2 = re.search(r"P2 wins:\s+(\d+)", out)
        if p1 and p2:
            return int(p1.group(1)) if deck_is_p1 else int(p2.group(1))
        single = re.search(r"Game over: winner=(\d)", out)   # --games 1 prints this instead
        if single:
            return int(single.group(1) == ("1" if deck_is_p1 else "2"))
        raise RuntimeError("engine run failed:\n" + out[-800:])

    def winrates(self, items: list[tuple[dict, int, str]]) -> list[tuple[int, int]]:
        """(wins, games) for each (deck, games, tag), alternating who starts."""
        jobs, owner, totals = [], [], []
        for k, (deck, games, tag) in enumerate(items):
            path = self.workdir / "decks" / f"{tag}.txt"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(render_deck(deck), encoding="utf-8")
            half = max(1, games // 2)
            totals.append(2 * half)
            for first in (True, False):
                left = half
                while left > 0:
                    g = min(self.chunk, left)
                    left -= g
                    jobs.append((path, self.opponent, g, True) if first
                                else (self.opponent, path, g, False))
                    owner.append(k)
        wins = [0] * len(items)
        with ThreadPoolExecutor(max_workers=self.workers) as ex:
            for k, w in zip(owner, ex.map(self._run, jobs)):
                wins[k] += w
        self.games_played += sum(totals)
        return list(zip(wins, totals))

    def winrate(self, deck: dict, games: int, tag: str) -> tuple[int, int]:
        return self.winrates([(deck, games, tag)])[0]


def better(w1: int, n1: int, w0: int, n0: int, z: float = 1.6449) -> bool:
    """One-sided test that rate 1 > rate 0 (z=1.6449 is ~95% confidence)."""
    p1, p0 = w1 / n1, w0 / n0
    p = (w1 + w0) / (n1 + n0)
    se = math.sqrt(max(p * (1 - p) * (1 / n1 + 1 / n0), 1e-12))
    return (p1 - p0) / se > z


# ── Search ───────────────────────────────────────────────────────────────────
def propose(deck: dict, candidates: list[str], k: int, rng: random.Random,
            tried: set[str]) -> list[tuple[dict, str]]:
    """Up to k one-copy swaps (out -> in) that keep the list legal."""
    main = deck["maindeck"]
    champion = next(iter(deck["champion"]), None)
    outs = [n for n in main if n != champion]
    variants, attempts = [], 0
    # Bias the "in" side towards the front of the priority list.
    weights = [1.0 / (1 + i / 15) for i in range(len(candidates))]
    while len(variants) < k and attempts < k * 50:
        attempts += 1
        add = rng.choices(candidates, weights=weights, k=1)[0]
        out = rng.choice(outs)
        if add == out:
            continue
        copies = main[add] + deck["champion"][add] + deck["sideboard"][add]
        if copies >= 3:
            continue
        v = copy_deck(deck)
        v["maindeck"][out] -= 1
        if v["maindeck"][out] == 0:
            del v["maindeck"][out]
        v["maindeck"][add] += 1
        key = deck_key(v)
        if key in tried:
            continue
        tried.add(key)
        variants.append((v, f"-1 {out} / +1 {add}"))
    return variants


def write_report(out: Path, state: dict, args) -> None:
    lines = [
        f"# Búsqueda de mazo: {Path(args.deck).stem} contra {Path(args.opponent).stem}",
        "",
        f"Agente: `{args.agent}`. Partidas jugadas en total: {state['games_total']}. "
        f"Rondas: {state['rounds']}. Última actualización: {time.strftime('%Y-%m-%d %H:%M')}.",
        "",
        "| Lista | Victorias | Partidas | Win rate |",
        "|---|---|---|---|",
        f"| Original | {state['start_wins']} | {state['start_games']} | "
        f"{100 * state['start_wins'] / max(1, state['start_games']):.1f}% |",
        f"| Mejor encontrada | {state['best_wins']} | {state['best_games']} | "
        f"{100 * state['best_wins'] / max(1, state['best_games']):.1f}% |",
        "",
        "## Cambios aceptados",
        "",
    ]
    if state["accepted"]:
        for a in state["accepted"]:
            lines.append(f"- Ronda {a['round']}: {a['swap']} "
                         f"({a['wins']}/{a['games']} = {100 * a['wins'] / a['games']:.1f}% contra "
                         f"{a['prev_wins']}/{a['prev_games']} = "
                         f"{100 * a['prev_wins'] / a['prev_games']:.1f}%)")
    else:
        lines.append("- Ninguno todavía: ninguna variante ganó con suficiente margen.")
    lines += ["", "## Mejor lista", "", "```", render_deck(state["best_deck"]).rstrip(), "```", "",
              "Los bots MCTS ven la mano rival y juegan peor que un humano bueno: "
              "toma el resultado como una pista fuerte, no como una prueba."]
    (out / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--deck", required=True, help="deck list to improve")
    ap.add_argument("--opponent", required=True, help="fixed opponent deck list")
    ap.add_argument("--agent", default="mcts:sims=20", help="engine agent spec for both players")
    ap.add_argument("--hours", type=float, default=7.0, help="time budget")
    ap.add_argument("--variants", type=int, default=24, help="swaps tried per round")
    ap.add_argument("--start-games", type=int, default=40, help="games per variant in the first cut")
    ap.add_argument("--confirm-games", type=int, default=400, help="games to accept a swap")
    ap.add_argument("--out", default=None, help="output folder")
    ap.add_argument("--resume", action="store_true", help="continue a previous run in --out")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--workers", type=int, default=0, help="parallel engine runs (0 = one per core)")
    args = ap.parse_args()

    if not BINARY.exists():
        sys.exit(f"{BINARY} not found: build the engine first (see CLAUDE.md).")
    deck_path, opp_path = Path(args.deck).resolve(), Path(args.opponent).resolve()
    out = Path(args.out or ROOT / "deck_search_runs" / f"{deck_path.stem}_vs_{opp_path.stem}")
    out.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed or None)
    deadline = time.time() + args.hours * 3600

    cards = load_registry(out / "registry.json")
    base = parse_deck(deck_path.read_text(encoding="utf-8"))
    if not base["legend"]:
        sys.exit("The deck has no Legend section.")
    legend = find_legend(cards, next(iter(base["legend"])))
    pool = legal_pool(cards, legend)
    candidates = candidate_order(base, pool, rng)
    sim = Simulator(opp_path, args.agent, out, timeout=6 * 3600,
                    workers=args.workers or os.cpu_count() or 4)
    print(f"Legend {legend['name']} {legend.get('domains')}: {len(pool)} legal cards.", flush=True)

    state_file = out / "state.json"
    if args.resume and state_file.exists():
        state = json.loads(state_file.read_text())
        for s in SECTION_ORDER:
            state["best_deck"][s] = Counter(state["best_deck"][s])
        tried = set(state["tried"])
        games_offset = state["games_total"]
        print(f"Resuming: round {state['rounds']}, {state['games_total']} games so far.", flush=True)
    else:
        print(f"Measuring the starting list ({args.confirm_games} games)...", flush=True)
        w, n = sim.winrate(base, args.confirm_games, "start")
        sim.games_played = 0
        state = {"best_deck": base, "best_wins": w, "best_games": n, "start_wins": w,
                 "start_games": n, "accepted": [], "rounds": 0, "games_total": n, "tried": []}
        tried = {deck_key(base)}
        games_offset = n
        print(f"Starting list: {w}/{n} = {100 * w / n:.1f}%", flush=True)

    def save() -> None:
        s = dict(state)
        s["best_deck"] = {k: dict(v) if isinstance(v, Counter) else v
                          for k, v in state["best_deck"].items()}
        s["tried"] = sorted(tried)
        state_file.write_text(json.dumps(s, ensure_ascii=False, indent=1))
        (out / "best_deck.txt").write_text(render_deck(state["best_deck"]), encoding="utf-8")
        write_report(out, state, args)

    save()
    history = (out / "history.jsonl").open("a", encoding="utf-8")
    while time.time() < deadline:
        state["rounds"] += 1
        rnd = state["rounds"]
        pool_now = propose(state["best_deck"], candidates, args.variants, rng, tried)
        if not pool_now:
            print("No untried swaps left.", flush=True)
            break
        # Successive halving: few games for many variants, more for the best.
        alive = [{"deck": d, "swap": s, "w": 0, "n": 0} for d, s in pool_now]
        games = args.start_games
        while len(alive) > 1 and time.time() < deadline:
            results = sim.winrates([(v["deck"], games, f"r{rnd}_{i}") for i, v in enumerate(alive)])
            for v, (w, n) in zip(alive, results):
                v["w"] += w
                v["n"] += n
                history.write(json.dumps({"round": rnd, "swap": v["swap"], "wins": v["w"],
                                          "games": v["n"]}, ensure_ascii=False) + "\n")
            history.flush()
            alive.sort(key=lambda v: v["w"] / v["n"], reverse=True)
            alive = alive[:max(1, len(alive) // 4)]
            games *= 2
        if time.time() >= deadline:
            break
        top = alive[0]
        # Confirm the round's winner with FRESH games only: the games that
        # selected it are biased upwards (it won the cut partly by luck).
        w, n = sim.winrate(top["deck"], args.confirm_games, f"r{rnd}_confirm")
        top["w"], top["n"] = w, n
        state["games_total"] = games_offset + sim.games_played
        msg = (f"Round {rnd}: best swap {top['swap']} -> {top['w']}/{top['n']} = "
               f"{100 * top['w'] / top['n']:.1f}% (current {100 * state['best_wins'] / state['best_games']:.1f}%)")
        passed = better(top["w"], top["n"], state["best_wins"], state["best_games"])
        if passed:
            # Many rounds at 95% let ~1 in 20 lucky variants through: replicate
            # with a second fresh batch and test both batches together.
            w2, n2 = sim.winrate(top["deck"], args.confirm_games, f"r{rnd}_replicate")
            top["w"] += w2
            top["n"] += n2
            passed = better(top["w"], top["n"], state["best_wins"], state["best_games"])
            msg += f"; replicated {top['w']}/{top['n']} = {100 * top['w'] / top['n']:.1f}%"
        if passed:
            state["accepted"].append({"round": rnd, "swap": top["swap"], "wins": top["w"],
                                      "games": top["n"], "prev_wins": state["best_wins"],
                                      "prev_games": state["best_games"]})
            state["best_deck"], state["best_wins"], state["best_games"] = top["deck"], top["w"], top["n"]
            msg += "  ACCEPTED"
        print(msg, flush=True)
        save()
    state["games_total"] = games_offset + sim.games_played
    save()
    print(f"Done. Report: {out / 'report.md'}", flush=True)


if __name__ == "__main__":
    main()
