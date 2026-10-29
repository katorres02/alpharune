#!/usr/bin/env bash
# Prepara una PC con Ubuntu (24.04, nativo o dentro de WSL2) para entrenar un
# agente de Riftbound con la GPU. Es seguro correrlo varias veces: lo que ya
# está hecho se salta.
#
# Uso:
#   bash setup_ubuntu.sh            # instala, clona, compila y prueba
#   bash setup_ubuntu.sh --no-tests # igual, sin correr la suite del motor
#
# Variables opcionales:
#   CODE_DIR   carpeta de trabajo            (por defecto ~/code)
#   REPO_URL   tu fork del motor             (por defecto el fork de katorres02)
#   BRANCH     rama a usar                   (por defecto radiance)
#
# Qué hace:
#   1. Comprueba que es Ubuntu y si corre dentro de WSL2.
#   2. Instala las dependencias para compilar el motor y para Python.
#   3. Comprueba la GPU NVIDIA (en Ubuntu nativo ofrece instalar el driver).
#   4. Instala Claude Code.
#   5. Clona o actualiza el motor, lo compila y corre sus tests.
#   6. Crea un entorno de Python con PyTorch para CUDA y verifica la GPU.
#   7. Deja preparada la carpeta del repo de entrenamiento.

set -euo pipefail

CODE_DIR="${CODE_DIR:-$HOME/code}"
REPO_URL="${REPO_URL:-https://github.com/katorres02/alpharune.git}"
BRANCH="${BRANCH:-radiance}"
ENGINE_DIR="$CODE_DIR/alpharune"
TRAINER_DIR="$CODE_DIR/riftbound-trainer"
VENV_DIR="$TRAINER_DIR/.venv"
RUN_TESTS=1
[[ "${1:-}" == "--no-tests" ]] && RUN_TESTS=0

step() { printf '\n\033[1;36m== %s\033[0m\n' "$*"; }
ok()   { printf '\033[1;32m   ok:\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m   aviso:\033[0m %s\n' "$*"; }
fail() { printf '\033[1;31m   error:\033[0m %s\n' "$*" >&2; exit 1; }
ask()  { local a; read -r -p "   $* [s/N] " a; [[ "$a" =~ ^[sSyY]$ ]]; }

# ── 1. Sistema ───────────────────────────────────────────────────────────────
step "1/7 Sistema"
[[ -r /etc/os-release ]] || fail "no encuentro /etc/os-release; ¿es Linux?"
. /etc/os-release
[[ "${ID:-}" == "ubuntu" ]] || fail "este script es para Ubuntu (encontré: ${PRETTY_NAME:-desconocido})"
IS_WSL=0
grep -qi microsoft /proc/version 2>/dev/null && IS_WSL=1
ok "$PRETTY_NAME$([[ $IS_WSL == 1 ]] && echo ' (dentro de WSL2)')"
[[ "${VERSION_ID:-}" == "24.04" ]] || warn "probado en Ubuntu 24.04; con ${VERSION_ID:-?} debería funcionar, pero no está probado"
MEM_GB=$(awk '/MemTotal/ {printf "%d", $2/1024/1024}' /proc/meminfo)
ok "RAM visible: ${MEM_GB} GB, núcleos: $(nproc)"
if [[ $IS_WSL == 1 && $MEM_GB -lt 16 ]]; then
    warn "WSL2 ve poca RAM. Puedes subirla creando C:\\Users\\<tu usuario>\\.wslconfig con:"
    warn "  [wsl2]"
    warn "  memory=24GB"
    warn "y luego 'wsl --shutdown' en PowerShell."
fi

# ── 2. Dependencias ──────────────────────────────────────────────────────────
step "2/7 Dependencias (pide tu contraseña de Ubuntu)"
sudo apt-get update -qq
sudo apt-get install -y -qq \
    build-essential g++ cmake ninja-build git curl ca-certificates \
    libboost-all-dev nlohmann-json3-dev \
    python3 python3-venv python3-dev python3-pip \
    nodejs npm cron
ok "paquetes instalados"

# ── 3. GPU ───────────────────────────────────────────────────────────────────
step "3/7 GPU NVIDIA"
if command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi >/dev/null 2>&1; then
    ok "$(nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader)"
elif [[ $IS_WSL == 1 ]]; then
    fail "WSL2 no ve la GPU. Actualiza el driver de NVIDIA en Windows (no dentro de Ubuntu), reinicia y vuelve a correr este script."
else
    warn "no hay driver de NVIDIA."
    if ask "¿Instalo el driver recomendado ahora? (después hay que reiniciar)"; then
        sudo apt-get install -y -qq ubuntu-drivers-common
        sudo ubuntu-drivers install
        warn "driver instalado. Reinicia la PC y vuelve a correr este script."
        exit 0
    fi
    fail "sin driver de NVIDIA no se puede entrenar con la GPU."
fi

# ── 4. Claude Code ───────────────────────────────────────────────────────────
step "4/7 Claude Code"
NODE_MAJOR=$(node -v 2>/dev/null | sed 's/^v\([0-9]*\).*/\1/' || echo 0)
if [[ "${NODE_MAJOR:-0}" -lt 18 ]]; then
    fail "Claude Code necesita Node.js 18 o más (encontré $(node -v 2>/dev/null || echo ninguno))."
fi
if command -v claude >/dev/null 2>&1; then
    ok "ya instalado: $(claude --version 2>/dev/null | head -1)"
else
    # Instalación global sin sudo: los paquetes van a ~/.npm-global.
    mkdir -p "$HOME/.npm-global"
    npm config set prefix "$HOME/.npm-global"
    if ! grep -q 'npm-global/bin' "$HOME/.bashrc" 2>/dev/null; then
        echo 'export PATH="$HOME/.npm-global/bin:$PATH"' >> "$HOME/.bashrc"
    fi
    export PATH="$HOME/.npm-global/bin:$PATH"
    npm install -g @anthropic-ai/claude-code >/dev/null
    ok "instalado: $(claude --version 2>/dev/null | head -1)"
fi

# ── 5. Motor ─────────────────────────────────────────────────────────────────
step "5/7 Motor de Riftbound ($BRANCH)"
mkdir -p "$CODE_DIR"
if [[ -d "$ENGINE_DIR/.git" ]]; then
    git -C "$ENGINE_DIR" fetch -q origin "$BRANCH"
    if [[ -n "$(git -C "$ENGINE_DIR" status --porcelain)" ]]; then
        warn "hay cambios locales en $ENGINE_DIR; no actualizo para no pisarlos"
    else
        git -C "$ENGINE_DIR" checkout -q "$BRANCH"
        git -C "$ENGINE_DIR" pull -q --ff-only origin "$BRANCH"
        ok "actualizado a $(git -C "$ENGINE_DIR" log --oneline -1)"
    fi
else
    git clone -q --branch "$BRANCH" "$REPO_URL" "$ENGINE_DIR"
    ok "clonado en $ENGINE_DIR"
fi
cd "$ENGINE_DIR"
if [[ ! -f build/build.ninja ]]; then
    echo "   configurando (la primera vez descarga OpenSpiel y sus dependencias)..."
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
fi
echo "   compilando (la primera vez tarda varios minutos)..."
cmake --build build >/dev/null
ok "motor compilado: build/riftbound"
if [[ $RUN_TESTS == 1 ]]; then
    echo "   corriendo los tests del motor..."
    if RIFTBOUND_ROOT=. ./build/riftbound_tests --gtest_brief=1 > /tmp/riftbound_tests.log 2>&1; then
        ok "$(grep -E 'PASSED' /tmp/riftbound_tests.log | tail -1)"
    else
        tail -20 /tmp/riftbound_tests.log
        fail "fallaron tests del motor (detalle en /tmp/riftbound_tests.log)"
    fi
fi
GAMES=$( { RIFTBOUND_ROOT=. ./build/riftbound --deck1 decks/darius_test.txt --deck2 decks/draven_test.txt --games 200 --threads 1 2>&1 || true; } | grep -c "^Game " || true)
[[ "$GAMES" -ge 200 ]] && ok "200 partidas de prueba jugadas" || warn "la prueba de partidas no terminó bien; revisa el motor"

# ── 6. Python + PyTorch ──────────────────────────────────────────────────────
step "6/7 Python y PyTorch con CUDA"
mkdir -p "$TRAINER_DIR"
if [[ ! -d "$VENV_DIR" ]]; then
    python3 -m venv "$VENV_DIR"
fi
# shellcheck disable=SC1091
source "$VENV_DIR/bin/activate"
python -m pip install -q --upgrade pip wheel
if ! python -c "import torch" 2>/dev/null; then
    echo "   instalando PyTorch con CUDA (descarga unos 2-3 GB)..."
    python -m pip install -q torch --index-url https://download.pytorch.org/whl/cu124
fi
python -m pip install -q numpy pybind11 tensorboard tqdm
python - <<'PY'
import torch, sys
if not torch.cuda.is_available():
    sys.exit("   error: PyTorch no ve la GPU (torch.cuda.is_available() es False)")
p = torch.cuda.get_device_properties(0)
x = torch.randn(4096, 4096, device="cuda")
y = (x @ x).sum().item()
print(f"   ok: PyTorch {torch.__version__} usa {p.name} ({p.total_memory // 2**20} MB)")
PY
deactivate

# ── 7. Carpeta del entrenamiento ─────────────────────────────────────────────
step "7/7 Repo de entrenamiento"
cd "$TRAINER_DIR"
[[ -d .git ]] || git init -q
mkdir -p logs reports checkpoints scripts
if [[ ! -f .gitignore ]]; then
    printf '.venv/\nbuild/\nlogs/\ncheckpoints/\nruns/\n__pycache__/\n' > .gitignore
fi
ok "$TRAINER_DIR listo (venv en .venv)"

cat <<EOF

$(printf '\033[1;32m')Todo listo.$(printf '\033[0m')

Siguiente paso: abre Claude Code y pégale el prompt de
  $ENGINE_DIR/docs/training/entrenamiento-nocturno.md

  cd $CODE_DIR && claude

(Si 'claude' no se encuentra, abre una terminal nueva o corre: source ~/.bashrc)

Cuando Claude Code haya creado scripts/train_night.sh, programa la corrida
nocturna con:

  bash $ENGINE_DIR/scripts/training/install_nightly.sh
EOF
