#!/usr/bin/env bash
# Corre la búsqueda de mazo de noche y retoma la anterior si existe.
#
# Uso:
#   bash scripts/deck_search/run_night.sh decks/draven_nuevo.txt decks/riven_radiance_namdang.txt [horas]
#
# El reporte queda en deck_search_runs/<mazo>_vs_<rival>/report.md.
set -euo pipefail
cd "$(dirname "$0")/../.."
DECK="${1:?falta el mazo a mejorar}"
OPP="${2:?falta el mazo rival}"
HOURS="${3:-7}"
OUT="deck_search_runs/$(basename "$DECK" .txt)_vs_$(basename "$OPP" .txt)"
mkdir -p "$OUT"
LOG="$OUT/night_$(date +%Y%m%d).log"
RESUME=""
[[ -f "$OUT/state.json" ]] && RESUME="--resume"
# Compila el motor si el código cambió (no hace nada si ya está al día).
cmake --build build --target riftbound >> "$LOG" 2>&1
python3 scripts/deck_search/deck_search.py --deck "$DECK" --opponent "$OPP" \
    --hours "$HOURS" --out "$OUT" $RESUME >> "$LOG" 2>&1
echo "Listo: $OUT/report.md" | tee -a "$LOG"
