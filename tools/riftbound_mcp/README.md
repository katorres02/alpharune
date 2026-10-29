# Riftbound MCP + local model (Ollama)

A Riftbound assistant that runs for free on your machine: a local model (Ollama)
backed by real tools for cards, decks, probabilities and simulations. The MCP
server can also be plugged into any MCP client (Claude Desktop, Claude Code, ...).

## Tools
| Tool | What it does |
|---|---|
| help | Lists every tool, the domain colors, the set codes and the deck format |
| get_card | Official text and stats of one card; understands partial names, colors and sets |
| search_cards | Search by name/tag/text, type, domain or color, cost, set |
| list_decks | Decks saved in `decks/` |
| validate_deck | Basic deck-building rules |
| analyze_deck | Curve, types, domains and the odds of seeing each card |
| draw_probability | Exact hypergeometric probability |
| simulate_matchup | Simulates A vs B with alpharune, alternating seats |
| update_cards | Downloads the official Riot gallery (internet) |

### Colors and ambiguity
Players call domains by color, and `get_card` understands it: red = Fury,
green = Calm, blue = Mind, orange = Body, purple = Chaos, yellow = Order
(Spanish synonyms work too: rojo, verde, azul, naranja, morado, amarillo).

`get_card` returns a `status`:
- `ok`: one card, with `other_printings` when it was reprinted.
- `ambiguous`: several cards share the words you gave ("Darius" matches
  Darius, Trifarian and Darius, Executioner) or the card was printed in several
  sets. The response carries `candidates` and a `question`; the model is told
  to ask you which one you mean rather than guess. Add a color, a type
  ("Riven legend") or a set ("Jinx Demolitionist Vendetta") to resolve it.
- `not_found`: close name suggestions, or the candidates that your filters
  excluded.

Set codes: OGN Origins, OGS Proving Grounds, SFD Spiritforged, UNL Unleashed,
VEN Vendetta, RAD Radiance.

### Card data
Cards come from `cards/card_index.json` (official gallery). When the engine is
built, cards compiled into it but missing from the gallery (e.g. a Legend the
gallery does not list yet) are merged in from `riftbound --dump-registry`,
marked with `"source": "engine"`.

## Install (Mac)
From the alpharune root, with the engine already built:

    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r tools/riftbound_mcp/requirements.txt
    ollama pull qwen3.5:4b
    ollama create riftbound -f tools/riftbound_mcp/Modelfile
    python3 tools/riftbound_mcp/chat.py

Every new terminal: `source .venv/bin/activate`. Inside the chat, `/help`
prints the tool list locally and `exit` quits.

### Using the server from another MCP client
Run `python3 tools/riftbound_mcp/server.py` over stdio. Set `RIFTBOUND_ROOT`
if the server does not live inside the alpharune checkout.

## Tuning the model
1. `system_prompt.md`: rules and hints (the highest-impact change).
2. Tool descriptions in `server.py` (the model reads them to decide).
3. `Modelfile`: base model, temperature and context. After a change: `ollama create riftbound -f Modelfile`.
4. Always measure: `python3 eval.py --model riftbound`. Add cases to `eval_cases.json`.
5. `python3 chat.py --log` saves conversations to `logs/`: raw material for a future fine-tune.
