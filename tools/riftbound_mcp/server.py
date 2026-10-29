"""Riftbound MCP server.

Tools: search and look up cards, list/validate/analyze decks, compute draw
probabilities and simulate matchups with the alpharune engine.

Card source: cards/card_index.json in the repo (official Riot gallery).
Refresh it with `python3 scripts/fetch_cards.py` or the `update_cards` tool.
"""
from __future__ import annotations

import difflib
import json
import math
import os
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

from mcp.server.mcpserver import MCPServer

ROOT = Path(os.environ.get("RIFTBOUND_ROOT", Path(__file__).resolve().parents[2])).resolve()
CARD_INDEX = ROOT / "cards" / "card_index.json"
DECKS_DIR = ROOT / "decks"
BINARY = ROOT / "build" / "riftbound"
REGISTRY_CACHE = Path(__file__).resolve().parent / ".engine_registry.json"

server = MCPServer(
    "riftbound",
    instructions=(
        "Tools for the Riftbound TCG. Always use these tools for card text, deck "
        "lists, probabilities and simulation results; never make data up. Call "
        "`help` to see every tool, the domain colors and the set codes. When "
        "`get_card` returns status 'ambiguous', ask the user which candidate or "
        "which set they mean instead of guessing."
    ),
)

# ─── Vocabulary: domains, colors, sets, card types ───────────────────────────
DOMAINS = ["fury", "calm", "mind", "body", "chaos", "order"]

# Players refer to domains by color ("red Darius" = Darius, Fury). Includes the
# official color of each domain plus English and Spanish synonyms.
DOMAIN_ALIASES: dict[str, str] = {
    "fury": "fury", "furia": "fury", "red": "fury", "rojo": "fury", "roja": "fury",
    "calm": "calm", "calma": "calm", "green": "calm", "verde": "calm",
    "mind": "mind", "mente": "mind", "blue": "mind", "azul": "mind",
    "body": "body", "cuerpo": "body", "orange": "body", "naranja": "body", "naranjo": "body",
    "chaos": "chaos", "caos": "chaos", "purple": "chaos", "morado": "chaos", "morada": "chaos",
    "violet": "chaos", "violeta": "chaos", "purpura": "chaos", "púrpura": "chaos",
    "order": "order", "orden": "order", "yellow": "order", "amarillo": "order", "amarilla": "order",
}
DOMAIN_COLORS = {"fury": "red", "calm": "green", "mind": "blue", "body": "orange", "chaos": "purple", "order": "yellow"}

SETS: dict[str, str] = {
    "OGN": "Origins", "OGS": "Proving Grounds", "SFD": "Spiritforged",
    "UNL": "Unleashed", "VEN": "Vendetta", "RAD": "Radiance",
}
SET_ALIASES: dict[str, str] = {
    "ogn": "OGN", "origins": "OGN", "origenes": "OGN", "orígenes": "OGN",
    "ogs": "OGS", "proving": "OGS", "provinggrounds": "OGS",
    "sfd": "SFD", "spiritforged": "SFD",
    "unl": "UNL", "unleashed": "UNL",
    "ven": "VEN", "vendetta": "VEN",
    "rad": "RAD", "radiance": "RAD", "radiancia": "RAD",
}

CARD_TYPES = ["unit", "spell", "gear", "battlefield", "legend", "rune"]
TYPE_ALIASES: dict[str, str] = {
    "unit": "unit", "unidad": "unit", "units": "unit", "unidades": "unit",
    "spell": "spell", "spells": "spell", "hechizo": "spell", "hechizos": "spell", "conjuro": "spell",
    "gear": "gear", "equipo": "gear", "equipment": "gear",
    "battlefield": "battlefield", "battlefields": "battlefield", "campo": "battlefield",
    "legend": "legend", "legends": "legend", "leyenda": "legend", "leyendas": "legend",
    "rune": "rune", "runes": "rune", "runa": "rune", "runas": "rune",
    # super types are filters on their own
    "champion": "champion", "champ": "champion", "campeon": "champion", "campeón": "champion",
    "signature": "signature", "firma": "signature",
}
STOPWORDS = {"the", "of", "a", "an", "card", "carta", "el", "la", "los", "las", "de", "del", "version", "versión"}

# ─── Card data ───────────────────────────────────────────────────────────────
_cards: list[dict] = []
_by_name: dict[str, list[dict]] = {}   # normalized name -> every printing


def _norm(s: str) -> str:
    s = s.lower().replace("’", "'")
    return re.sub(r"\s+", " ", s).strip()


def _engine_cards() -> list[dict]:
    """Cards compiled into the engine (`riftbound --dump-registry`).

    The gallery index can lag behind the engine (e.g. a Legend missing from
    the official gallery), so these fill the gaps. Best effort: returns []
    when the binary is not built.
    """
    if not BINARY.exists():
        return []
    try:
        if not REGISTRY_CACHE.exists() or REGISTRY_CACHE.stat().st_mtime < BINARY.stat().st_mtime:
            subprocess.run([str(BINARY), "--dump-registry", str(REGISTRY_CACHE)], cwd=ROOT, check=True,
                           capture_output=True, timeout=120, env={**os.environ, "RIFTBOUND_ROOT": str(ROOT)})
        data = json.loads(REGISTRY_CACHE.read_text(encoding="utf-8"))
        cards = data.get("cards", data) if isinstance(data, dict) else data
        return list(cards.values()) if isinstance(cards, dict) else list(cards)
    except (OSError, ValueError, subprocess.SubprocessError):
        return []


def _load_cards() -> None:
    global _cards, _by_name
    _cards = json.loads(CARD_INDEX.read_text(encoding="utf-8"))
    known = {c.get("public_code") for c in _cards}
    for c in _engine_cards():
        if c.get("public_code") and c["public_code"] not in known:
            _cards.append({**c, "source": "engine"})
            known.add(c["public_code"])
    _by_name = defaultdict(list)
    for c in _cards:
        _by_name[_norm(c["name"])].append(c)
        # Legends are written "Champion, Title" in deck lists (e.g. "Riven, The Exile")
        if c.get("card_type") == "legend":
            for tag in c.get("tags") or []:
                _by_name[_norm(f"{tag}, {c['name']}")].append(c)


_load_cards()


def _find(name: str) -> dict | None:
    """Exact-name lookup used by the deck tools (first printing wins)."""
    hits = _by_name.get(_norm(name))
    return hits[0] if hits else None


def _domain(value: str) -> str | None:
    return DOMAIN_ALIASES.get(_norm(value).replace(" ", "")) if value else None


def _set_code(value: str) -> str | None:
    return SET_ALIASES.get(_norm(value).replace(" ", "")) if value else None


def _summary(c: dict, full: bool = False) -> dict:
    out = {
        "name": c["name"],
        "type": c.get("card_type"),
        "super_type": c.get("super_type"),
        "domains": c.get("domains"),
        "energy": c.get("energy_cost"),
        "power": c.get("power_cost"),
        "might": c.get("might"),
        "set": c.get("set"),
        "code": c.get("public_code"),
    }
    if c.get("source"):
        out["source"] = c["source"]
    if full:
        out.update(
            {
                "tags": c.get("tags"),
                "banned": c.get("banned"),
                "rarity": c.get("rarity"),
                "text": c.get("ability_text"),
                "effect_text": c.get("effect_text"),
                "might_bonus": c.get("might_bonus"),
                "image": c.get("image_url"),
            }
        )
    else:
        txt = (c.get("ability_text") or "") + " " + (c.get("effect_text") or "")
        out["text"] = txt.strip()[:220]
    return out


def _printing_key(c: dict) -> tuple:
    return (c.get("ability_text"), c.get("effect_text"), c.get("energy_cost"), c.get("power_cost"), c.get("might"))


# ─── Card lookup with disambiguation ─────────────────────────────────────────
def _parse_query(text: str) -> tuple[list[str], set[str], str | None, set[str]]:
    """Split a free-form card reference into name words and filters.

    "red darius" -> (["darius"], {"fury"}, None, set())
    "darius legend vendetta" -> (["darius"], set(), "VEN", {"legend"})
    """
    words = [w for w in re.split(r"[\s,]+", _norm(text)) if w]
    names: list[str] = []
    domains: set[str] = set()
    types: set[str] = set()
    set_code: str | None = None
    for w in words:
        if w in DOMAIN_ALIASES:
            domains.add(DOMAIN_ALIASES[w])
        elif w in SET_ALIASES:
            set_code = SET_ALIASES[w]
        elif w in TYPE_ALIASES:
            types.add(TYPE_ALIASES[w])
        elif w not in STOPWORDS:
            names.append(w)
    return names, domains, set_code, types


def _matches_filters(c: dict, domains: set[str], set_code: str | None, types: set[str]) -> bool:
    if domains and not domains <= set(c.get("domains") or []):
        return False
    if set_code and c.get("set") != set_code:
        return False
    for t in types:
        if t in ("champion", "signature"):
            if c.get("super_type") != t:
                return False
        elif c.get("card_type") != t:
            return False
    return True


def _words(text: str) -> list[str]:
    return [w for w in re.split(r"[\s,]+", _norm(text)) if w]


def _has_words(words: list[str], query: list[str]) -> bool:
    """Every query word is a word (or a word prefix) of `words`."""
    return all(any(w == q or w.startswith(q) for w in words) for q in query)


def _candidate_cards(raw: str, names: list[str], domains: set[str], set_code: str | None, types: set[str]) -> list[dict]:
    """Cards matching every name word, exact name first.

    Matches on the printed name win over matches on name + tags, so "red
    Darius" resolves to the unit Darius, Trifarian and not to the Fury
    Legend/Signature that merely carry the Darius tag, while "Riven legend"
    still reaches the Legend "The Exile" through its Riven tag.
    """
    for key in (raw, " ".join(names)):
        exact = [c for c in _by_name.get(key, []) if _matches_filters(c, domains, set_code, types)]
        if exact:
            return exact
    by_name, by_tag = [], []
    for c in _cards:
        if not _matches_filters(c, domains, set_code, types):
            continue
        name_words = _words(c["name"])
        if _has_words(name_words, names):
            by_name.append(c)
        elif _has_words(name_words + _words(" ".join(c.get("tags") or [])), names):
            by_tag.append(c)
    return by_name or by_tag


@server.tool()
def get_card(name: str, card_set: str = "", domain: str = "") -> dict:
    """Look up one card and return its full official text and stats.

    `name` may be partial or informal: "Darius", "red Darius", "Riven legend",
    "Jinx Demolitionist vendetta". Colors map to domains (red=fury, green=calm,
    blue=mind, orange=body, purple=chaos, yellow=order). `card_set` (OGN, SFD,
    UNL, VEN, RAD, OGS or the set name) and `domain` narrow the match.

    Returns `status`: "ok" (one card; `other_printings` lists reprints),
    "ambiguous" (several cards or several printings match: show the
    `candidates` to the user and ask which one they mean), or "not_found"
    (`suggestions` lists close names).
    """
    names, domains, set_code, types = _parse_query(name)
    if domain:
        d = _domain(domain)
        if not d:
            return {"status": "error", "error": f"Unknown domain '{domain}'. Use one of {DOMAINS} or a color."}
        domains.add(d)
    if card_set:
        s = _set_code(card_set)
        if not s:
            return {"status": "error", "error": f"Unknown set '{card_set}'. Use one of {list(SETS)}."}
        set_code = s
    if not names:
        return {"status": "error", "error": "Give at least part of the card name."}

    cands = _candidate_cards(_norm(name), names, domains, set_code, types)
    if not cands:
        out = {"status": "not_found", "query": name}
        unfiltered = _candidate_cards(_norm(name), names, set(), None, set())
        if unfiltered:
            # The name exists but the color/set/type filter ruled everything out
            out["reason"] = "the name matches cards, but none with " + ", ".join(
                filter(None, [f"domain {sorted(domains)}" if domains else "", f"set {set_code}" if set_code else "",
                              f"type {sorted(types)}" if types else ""]))
            out["question"] = "Did you mean one of these? Ask the user to confirm."
            out["candidates"] = [_summary(c) for c in unfiltered[:12]]
        else:
            pool = sorted({c["name"] for c in _cards})
            out["suggestions"] = difflib.get_close_matches(" ".join(names), pool, n=5, cutoff=0.5)
        return out

    by_name: dict[str, list[dict]] = defaultdict(list)
    for c in cands:
        by_name[c["name"]].append(c)

    if len(by_name) > 1:
        return {
            "status": "ambiguous",
            "reason": "several cards match",
            "question": "Which of these cards do you mean? You can add the domain/color, the type (unit, legend, spell...) or the set.",
            "candidates": [_summary(c) for c in cands[:12]],
            "more": max(0, len(cands) - 12),
        }

    printings = next(iter(by_name.values()))
    if len(printings) > 1:
        same_text = len({_printing_key(c) for c in printings}) == 1
        return {
            "status": "ambiguous",
            "reason": "several printings",
            "identical_text": same_text,
            "question": (
                f"{printings[0]['name']} was printed in {', '.join(f'{c['set']} ({SETS.get(c['set'], c['set'])})' for c in printings)}. "
                "Which set do you want?"
                + (" (The text is identical in every printing.)" if same_text else " (The text differs between printings.)")
            ),
            "candidates": [_summary(c) for c in printings],
        }

    card = printings[0]
    out = {"status": "ok", **_summary(card, full=True)}
    others = [c for c in _by_name.get(_norm(card["name"]), []) if c is not card]
    if others:
        out["other_printings"] = [{"set": c["set"], "code": c.get("public_code")} for c in others]
    return out


@server.tool()
def search_cards(
    text: str = "",
    type: str = "",
    domain: str = "",
    max_energy: int = -1,
    card_set: str = "",
    limit: int = 15,
) -> list[dict]:
    """Search cards by name, tag or ability text, with optional filters.

    type: unit, spell, gear, battlefield, legend, rune, champion, signature.
    domain: fury, calm, mind, body, chaos, order — or a color (red, green,
    blue, orange, purple, yellow).
    max_energy: maximum energy cost (-1 = no limit).
    card_set: OGN, SFD, UNL, VEN, RAD, OGS (or the set name).
    """
    q = _norm(text)
    d = _domain(domain) if domain else None
    if domain and not d:
        return [{"error": f"Unknown domain '{domain}'. Use one of {DOMAINS} or a color."}]
    s = _set_code(card_set) if card_set else None
    if card_set and not s:
        return [{"error": f"Unknown set '{card_set}'. Use one of {list(SETS)}."}]
    t = TYPE_ALIASES.get(_norm(type)) if type else None
    if type and not t:
        return [{"error": f"Unknown type '{type}'. Use one of {CARD_TYPES + ['champion', 'signature']}."}]
    res = []
    for c in _cards:
        if not _matches_filters(c, {d} if d else set(), s, {t} if t else set()):
            continue
        if max_energy >= 0 and (c.get("energy_cost") or 0) > max_energy:
            continue
        if q:
            hay = _norm(" ".join([c["name"], " ".join(c.get("tags") or []), c.get("ability_text") or "", c.get("effect_text") or ""]))
            if q not in hay:
                continue
        res.append(_summary(c))
        if len(res) >= limit:
            break
    return res


# ─── Decks ───────────────────────────────────────────────────────────────────
SECTIONS = {"legend", "champion", "maindeck", "battlefields", "runes", "sideboard", "neekolegends"}
SECTION_ALIASES = {"additionallegends": "neekolegends"}
# Cards that let a deck choose legends in addition to its starting legend
# (CR 103.1, Tournament Rules 401.2.a): normalized name -> how many.
ADDITIONAL_LEGEND_CARDS = {"neeko, blending in": 3}


def _deck_text(deck: str) -> tuple[str, str | None]:
    """Accepts a deck list as text, a file path, or a file name in decks/."""
    if "\n" in deck:
        return deck, None
    for p in (Path(deck), DECKS_DIR / deck, DECKS_DIR / f"{deck}.txt"):
        if p.is_file():
            return p.read_text(encoding="utf-8"), str(p)
    raise FileNotFoundError(f"Deck '{deck}' not found. Use list_decks.")


def _parse(text: str) -> dict[str, list[tuple[int, str]]]:
    deck: dict[str, list[tuple[int, str]]] = {s: [] for s in SECTIONS}
    sec = "maindeck"
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key = line.rstrip(":").replace(" ", "").lower()
        key = SECTION_ALIASES.get(key, key)
        if line.endswith(":") and key in SECTIONS:
            sec = key
            continue
        m = re.match(r"^(\d+)\s*x?\s+(.+)$", line)
        if m:
            deck[sec].append((int(m.group(1)), m.group(2).strip()))
    return deck


def _resolve_path(deck: str) -> str:
    """Returns a file path to hand to the simulator."""
    text, path = _deck_text(deck)
    if path:
        return path
    tmp = DECKS_DIR / "_mcp_tmp"
    tmp.mkdir(exist_ok=True)
    p = tmp / f"deck_{abs(hash(text)) % 10**8}.txt"
    p.write_text(text, encoding="utf-8")
    return str(p)


@server.tool()
def list_decks() -> list[dict]:
    """List the decks saved in decks/ with their Legend."""
    out = []
    for p in sorted(DECKS_DIR.glob("*.txt")):
        d = _parse(p.read_text(encoding="utf-8"))
        legend = d["legend"][0][1] if d["legend"] else "?"
        out.append({"file": p.name, "legend": legend})
    return out


@server.tool()
def validate_deck(deck: str) -> dict:
    """Check basic deck-building rules: exactly 1 Legend, a Champion of the
    same champion, 40 cards in the main deck (Champion included), at most 3
    copies, 12 runes, 3 battlefields, and domains matching the Legend.
    deck: list text, a file path, or a file name in decks/."""
    text, _ = _deck_text(deck)
    d = _parse(text)
    errors, warnings = [], []
    unknown = [n for s in SECTIONS for _, n in d[s] if not _find(n)]
    if unknown:
        errors.append(f"Unknown cards: {sorted(set(unknown))}")
    if sum(q for q, _ in d["legend"]) != 1:
        errors.append("There must be exactly 1 Legend.")
    legend = _find(d["legend"][0][1]) if d["legend"] else None
    domains = set(legend.get("domains") or []) if legend else set()
    tags = set(legend.get("tags") or []) if legend else set()
    if sum(q for q, _ in d["champion"]) != 1:
        errors.append("There must be exactly 1 Champion.")
    elif legend:
        ch = _find(d["champion"][0][1])
        if ch and not (set(ch.get("tags") or []) & tags):
            errors.append("The Champion does not match the Legend's champion.")
    main_total = sum(q for q, _ in d["maindeck"]) + sum(q for q, _ in d["champion"])
    if main_total < 40:
        errors.append(f"The main deck has {main_total} cards (minimum 40 including the Champion).")
    copies = Counter()
    for q, n in d["maindeck"] + d["champion"]:
        copies[_norm(n)] += q
    for n, q in copies.items():
        if q > 3:
            errors.append(f"{n}: {q} copies (maximum 3).")
    runes = sum(q for q, _ in d["runes"])
    if runes != 12:
        errors.append(f"{runes} runes (must be 12).")
    if len(d["battlefields"]) != 3:
        errors.append(f"{len(d['battlefields'])} battlefields (must be 3).")
    if domains:
        for q, n in d["maindeck"] + d["champion"]:
            c = _find(n)
            if c and c.get("domains") and not set(c["domains"]) <= domains:
                errors.append(f"{n} is {c['domains']}, outside the Legend's domains {sorted(domains)}.")
            if c and c.get("super_type") == "signature" and not (set(c.get("tags") or []) & tags):
                errors.append(f"{n} is another champion's Signature card.")
    # Legends chosen for Neeko, Blending In (section "Neeko Legends:").
    allowed = max((ADDITIONAL_LEGEND_CARDS.get(_norm(n), 0)
                   for _, n in d["maindeck"] + d["champion"] + d["sideboard"]), default=0)
    extra = d["neekolegends"]
    if extra:
        if not allowed:
            errors.append("Neeko Legends are declared, but Neeko, Blending In is not in the deck.")
        elif sum(q for q, _ in extra) > allowed:
            errors.append(f"{sum(q for q, _ in extra)} Neeko Legends (maximum {allowed}).")
        names = {legend["name"]} if legend else set()
        for q, n in extra:
            c = _find(n)
            if not c:
                continue
            if q != 1:
                errors.append(f"Neeko Legend {n} must be listed once.")
            if c.get("card_type") != "legend":
                errors.append(f"Neeko Legend {n} is not a legend.")
            elif legend and c["name"] == legend["name"]:
                errors.append(f"Neeko Legend {n} has the same name as the starting legend.")
            elif c["name"] in names:
                errors.append(f"Neeko Legend {n}: names must all be different.")
            names.add(c["name"])
    if sum(q for q, _ in d["sideboard"]) > 8:
        warnings.append("The sideboard has more than 8 cards.")
    warnings.append("Basic validation only: it does not cover every tournament rule.")
    return {"valid": not errors, "errors": errors, "warnings": warnings}


def _hyper_at_least(copies: int, draws: int, deck_size: int, minimum: int) -> float:
    total = math.comb(deck_size, draws)
    p = sum(
        math.comb(copies, k) * math.comb(deck_size - copies, draws - k)
        for k in range(minimum, min(copies, draws) + 1)
    )
    return p / total


@server.tool()
def draw_probability(copies: int, cards_seen: int, deck_size: int = 40, minimum: int = 1) -> dict:
    """Exact (hypergeometric) probability of holding at least `minimum` copies
    of a card after seeing `cards_seen` cards of the deck.
    Example: opening hand of 4 cards -> cards_seen=4."""
    if not (0 <= copies <= deck_size and 0 <= cards_seen <= deck_size):
        return {"error": "Values out of range."}
    p = _hyper_at_least(copies, cards_seen, deck_size, minimum)
    return {"probability": round(p, 4), "percent": f"{p * 100:.1f}%"}


@server.tool()
def analyze_deck(deck: str) -> dict:
    """Energy curve, card types, domains, per-card counts and the probability
    of seeing each card in the opening hand (4) and by turn 3 (~6 cards seen)."""
    text, _ = _deck_text(deck)
    d = _parse(text)
    main = d["maindeck"] + d["champion"]
    total = sum(q for q, _ in main)
    curve, types, dom = Counter(), Counter(), Counter()
    detail = []
    for q, n in main:
        c = _find(n)
        if not c:
            detail.append({"card": n, "copies": q, "error": "unknown"})
            continue
        e = c.get("energy_cost") or 0
        curve["7+" if e >= 7 else str(e)] += q
        types[c.get("card_type")] += q
        for x in c.get("domains") or ["colorless"]:
            dom[x] += q
        detail.append(
            {
                "card": c["name"],
                "copies": q,
                "energy": e,
                "type": c.get("card_type"),
                "in_opening_hand": f"{_hyper_at_least(q, 4, total, 1) * 100:.0f}%",
                "by_turn_3": f"{_hyper_at_least(q, min(6, total), total, 1) * 100:.0f}%",
            }
        )
    costs = [(_find(n) or {}).get("energy_cost") or 0 for q, n in main for _ in range(q)]
    return {
        "legend": d["legend"][0][1] if d["legend"] else None,
        "main_deck_cards": total,
        "energy_curve": dict(sorted(curve.items())),
        "average_cost": round(sum(costs) / max(1, len(costs)), 2),
        "types": dict(types),
        "domains": dict(dom),
        "battlefields": [n for _, n in d["battlefields"]],
        "runes": {n: q for q, n in d["runes"]},
        "cards": detail,
    }


# ─── Simulation ──────────────────────────────────────────────────────────────
AGENTS = {"random": "random", "mcts20": "mcts:sims=20", "mcts50": "mcts:sims=50"}


def _run(d1: str, d2: str, games: int, agent: str, timeout: int) -> tuple[int, int, int]:
    cmd = [str(BINARY), "--deck1", d1, "--deck2", d2, "--games", str(games), "--threads", "0",
           "--agent1", agent, "--agent2", agent]
    out = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout,
                         env={**os.environ, "RIFTBOUND_ROOT": str(ROOT)}).stdout

    def grab(label: str) -> int:
        m = re.search(rf"{label}:\s+(\d+)", out)
        return int(m.group(1)) if m else 0

    if "P1 wins" not in out:
        raise RuntimeError(out[-600:])
    return grab("P1 wins"), grab("P2 wins"), grab("Draws")


@server.tool()
def simulate_matchup(deck_a: str, deck_b: str, games: int = 200, agent: str = "random") -> dict:
    """Simulate deck_a against deck_b with the alpharune engine, alternating
    who goes first. agent: random (fast, up to 2000 games), mcts20 or mcts50
    (slower and more realistic, up to 200). The bots see the opponent's hand
    and do not play like humans: treat the result as an estimate."""
    if not BINARY.exists():
        return {"error": f"{BINARY} does not exist. Build alpharune first."}
    if agent not in AGENTS:
        return {"error": f"agent must be one of {list(AGENTS)}"}
    cap = 2000 if agent == "random" else 200
    games = max(2, min(games, cap))
    a, b = _resolve_path(deck_a), _resolve_path(deck_b)
    half = games // 2
    timeout = 600 if agent == "random" else 3600
    p1a, p2b, e1 = _run(a, b, half, AGENTS[agent], timeout)   # A goes first
    p1b, p2a, e2 = _run(b, a, half, AGENTS[agent], timeout)   # B goes first
    wins_a = p1a + p2a
    n = 2 * half
    wr = wins_a / n
    margin = 1.96 * math.sqrt(wr * (1 - wr) / n)
    return {
        "deck_a": Path(a).name,
        "deck_b": Path(b).name,
        "agent": agent,
        "games": n,
        "wins_a": wins_a,
        "wins_b": p2b + p1b,
        "draws": e1 + e2,
        "winrate_a": f"{wr * 100:.1f}%",
        "ci_95": f"{max(0, wr - margin) * 100:.1f}% – {min(1, wr + margin) * 100:.1f}%",
        "a_going_first": f"{p1a}/{half}",
        "a_going_second": f"{p2a}/{half}",
    }


@server.tool()
def update_cards() -> dict:
    """Download the official Riot gallery and refresh cards/card_index.json
    (needs internet). Useful when a new set is released."""
    r = subprocess.run(["python3", "scripts/fetch_cards.py"], cwd=ROOT,
                       capture_output=True, text=True, timeout=600)
    _load_cards()
    return {"ok": r.returncode == 0, "cards": len(_cards), "output": (r.stdout or r.stderr)[-400:]}


# ─── Help ────────────────────────────────────────────────────────────────────
TOOL_HELP = [
    {"tool": "help", "args": "", "what": "This overview: tools, domain colors, set codes, deck format."},
    {"tool": "get_card", "args": "name, [card_set], [domain]",
     "what": "Full text and stats of one card. Partial names, colors and set names are understood; "
             "returns status ok / ambiguous (ask the user) / not_found."},
    {"tool": "search_cards", "args": "[text], [type], [domain], [max_energy], [card_set], [limit]",
     "what": "Search by name, tag or ability text with filters."},
    {"tool": "list_decks", "args": "", "what": "Decks saved in decks/ with their Legend."},
    {"tool": "validate_deck", "args": "deck", "what": "Basic deck-building rules check."},
    {"tool": "analyze_deck", "args": "deck", "what": "Energy curve, types, domains and per-card draw odds."},
    {"tool": "draw_probability", "args": "copies, cards_seen, [deck_size=40], [minimum=1]",
     "what": "Exact hypergeometric probability."},
    {"tool": "simulate_matchup", "args": "deck_a, deck_b, [games=200], [agent=random|mcts20|mcts50]",
     "what": "Simulate A vs B with the alpharune engine, alternating seats."},
    {"tool": "update_cards", "args": "", "what": "Refresh the card index from the official gallery (internet)."},
]


@server.tool()
def help() -> dict:
    """List every tool with its arguments, the domain/color mapping, the set
    codes and the deck list format. Call this when the user asks what the
    assistant can do or which options are available."""
    return {
        "tools": TOOL_HELP,
        "domains": {d: {"color": DOMAIN_COLORS[d], "aliases": sorted(k for k, v in DOMAIN_ALIASES.items() if v == d and k != d)} for d in DOMAINS},
        "sets": SETS,
        "card_types": CARD_TYPES + ["champion (super type)", "signature (super type)"],
        "deck_format": "Sections 'Legend:', 'Champion:', 'MainDeck:', 'Battlefields:', 'Runes:', 'Sideboard:' followed by lines '<count> <card name>'. With Neeko, Blending In, an optional 'Neeko Legends:' section lists up to 3 extra legends ('1 <legend>' each). Legends may be written 'Champion, Title'.",
        "cards_loaded": len(_cards),
        "engine_built": BINARY.exists(),
    }


if __name__ == "__main__":
    server.run("stdio")
