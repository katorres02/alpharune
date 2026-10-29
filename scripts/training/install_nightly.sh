#!/usr/bin/env bash
# Programa el entrenamiento nocturno con cron (Ubuntu nativo o WSL2).
#
# Uso:
#   bash install_nightly.sh            # todas las noches a la 1:00
#   bash install_nightly.sh 23 30      # todas las noches a las 23:30
#   bash install_nightly.sh --remove   # quita la programación
#
# Llama a ~/code/riftbound-trainer/scripts/train_night.sh (lo crea Claude
# Code con el prompt de docs/training/entrenamiento-nocturno.md) y guarda la
# salida en ~/code/riftbound-trainer/logs/cron.log.

set -euo pipefail

CODE_DIR="${CODE_DIR:-$HOME/code}"
TRAINER_DIR="$CODE_DIR/riftbound-trainer"
SCRIPT="$TRAINER_DIR/scripts/train_night.sh"
TAG="# riftbound-nightly"

current_crontab() { crontab -l 2>/dev/null || true; }

if [[ "${1:-}" == "--remove" ]]; then
    current_crontab | grep -v "$TAG" | crontab -
    echo "Programación nocturna quitada."
    exit 0
fi

HOUR="${1:-1}"
MINUTE="${2:-0}"
[[ "$HOUR" =~ ^[0-9]+$ && "$HOUR" -le 23 ]] || { echo "Hora inválida: $HOUR" >&2; exit 1; }
[[ "$MINUTE" =~ ^[0-9]+$ && "$MINUTE" -le 59 ]] || { echo "Minuto inválido: $MINUTE" >&2; exit 1; }

if [[ ! -x "$SCRIPT" ]]; then
    echo "No encuentro $SCRIPT (o no es ejecutable)." >&2
    echo "Primero pídele a Claude Code que lo cree con el prompt de la guía." >&2
    exit 1
fi

mkdir -p "$TRAINER_DIR/logs"
LINE="$MINUTE $HOUR * * * /bin/bash -lc '$SCRIPT' >> '$TRAINER_DIR/logs/cron.log' 2>&1 $TAG"
{ current_crontab | grep -v "$TAG"; echo "$LINE"; } | crontab -
printf 'Programado todas las noches a las %02d:%02d.\n' "$HOUR" "$MINUTE"

# cron tiene que estar corriendo. En Ubuntu nativo lo está siempre; en WSL2 solo
# mientras WSL esté encendido.
if grep -qi microsoft /proc/version 2>/dev/null; then
    if ! pgrep -x cron >/dev/null 2>&1; then
        sudo service cron start >/dev/null 2>&1 || true
    fi
    cat <<'EOF'

Importante en WSL2: cron solo corre mientras WSL está encendido, y Windows lo
apaga cuando no hay terminales abiertas. Para que corra de noche, haz una de
estas dos cosas:

  A) Usa el Programador de tareas de Windows en vez de cron (recomendado):
     programa:   wsl.exe
     argumentos: -d Ubuntu-24.04 -- bash -lc "~/code/riftbound-trainer/scripts/train_night.sh"
     (y quita esta programación con: bash install_nightly.sh --remove)

  B) Deja una terminal de Ubuntu abierta toda la noche y la PC sin suspender.
EOF
else
    sudo systemctl enable --now cron >/dev/null 2>&1 || true
    echo "cron está activo. La PC debe estar encendida y sin suspender a esa hora."
fi
echo
echo "Ver lo programado:  crontab -l"
echo "Ver el log:         tail -f $TRAINER_DIR/logs/cron.log"
