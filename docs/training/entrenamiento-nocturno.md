# Entrenar un agente de Riftbound en tu PC, de noche

Guía para entrenar tu propio agente con el motor de este repo en una PC con
Windows, una RTX 4060 Ti de 8 GB y 32 GB de RAM. Claude Code arma todo la
primera vez; después la PC entrena sola cada noche.

## ¿Sirve tu PC?

Sí. Para este proyecto es una buena máquina.

| Pieza | Para qué se usa | ¿Alcanza? |
|---|---|---|
| RTX 4060 Ti 8 GB | Entrenar la red neuronal y evaluar jugadas | Sí. La red que hace falta pesa decenas de MB, no GB |
| 32 GB de RAM | Guardar las partidas jugadas antes de aprender de ellas | Sí, con margen |
| CPU | Correr el motor de juego | Sí. El motor juega unas 400 partidas por segundo en un solo núcleo |

Expectativas realistas:

- La primera noche el agente debería empezar a ganarle al jugador aleatorio.
- Ganarle a MCTS (el agente de búsqueda que ya trae el motor) puede tomar
  varias semanas de noches.
- No hay garantía de que llegue a jugar como un humano bueno. Es un
  experimento, y cada mañana vas a ver un número que dice si va mejorando.

## Qué se va a construir

No existe un modelo entrenado que puedas reusar: el repo de entrenamiento del
autor original es privado. Vas a entrenar uno propio desde cero, en un repo
nuevo, `riftbound-trainer`, al lado de este.

1. **Un puente entre el motor (C++) y Python.** El motor ya implementa el juego
   con la interfaz de OpenSpiel: dice qué acciones son legales, qué ve cada
   jugador (la "observación", 10688 números) y quién ganó. Falta exponerlo a
   Python con `pybind11`, para que PyTorch pueda jugar partidas.
2. **El agente: una red neuronal** que recibe la observación y devuelve una
   probabilidad para cada acción legal, más una estimación de quién va ganando.
3. **El entrenamiento: auto-juego con PPO.** El agente juega contra versiones
   anteriores de sí mismo. Las jugadas que llevaron a ganar se vuelven más
   probables y las que llevaron a perder, menos. PPO es un método estándar,
   estable y fácil de depurar.
4. **La evaluación:** al final de cada noche, el agente juega contra el
   aleatorio y contra MCTS, y se guarda el porcentaje de victorias.
5. **El horario:** Windows lanza el entrenamiento a la 1 a. m. y lo corta a las
   7 a. m. Cada noche retoma donde quedó la anterior.

## Paso 1: preparar Windows (una sola vez, a mano)

El motor se compila en Linux, así que se usa WSL2 (Linux dentro de Windows).
La GPU funciona dentro de WSL2 sin instalar drivers adicionales en Linux.

1. Actualiza el driver de NVIDIA en Windows (GeForce Experience o nvidia.com).
2. Abre PowerShell como administrador y ejecuta:

   ```powershell
   wsl --install -d Ubuntu-24.04
   ```

   Reinicia cuando lo pida y crea tu usuario de Ubuntu.
3. En la terminal de Ubuntu, corre el script de instalación. Instala las
   dependencias, comprueba la GPU, instala Claude Code, clona y compila el
   motor, corre sus tests y prepara PyTorch con CUDA. Es seguro correrlo de
   nuevo si algo falla a la mitad.

   ```bash
   curl -fsSL https://raw.githubusercontent.com/katorres02/alpharune/radiance/scripts/training/setup_ubuntu.sh -o setup_ubuntu.sh
   bash setup_ubuntu.sh
   ```

   Si dice que no ve la GPU, actualiza el driver de NVIDIA en Windows y vuelve
   a correrlo. El mismo script sirve en una PC con Ubuntu instalado
   directamente, sin Windows; ahí ofrece instalar el driver de NVIDIA.
4. En Windows, evita que la PC se duerma de noche: Configuración → Sistema →
   Energía → "Suspender" en "Nunca" mientras esté enchufada.

## Paso 2: que Claude Code construya el entrenamiento

Abre Claude Code en Ubuntu, en la carpeta `~/code`, y pégale el prompt de la
sección siguiente. La primera sesión puede tomar unas horas. Quédate cerca
para aprobar comandos y responder preguntas. Termina cuando la prueba corta
(paso 6 del prompt) funcione.

```bash
cd ~/code && claude
```

## Paso 3: dejarlo corriendo de noche

Claude Code va a crear `~/code/riftbound-trainer/scripts/train_night.sh`.
Programa su ejecución en Windows:

1. Abre el **Programador de tareas** de Windows → "Crear tarea básica".
2. Nombre: `Riftbound entrenamiento`. Desencadenador: diario, 1:00 a. m.
3. Acción: "Iniciar un programa".
   - Programa: `wsl.exe`
   - Argumentos: `-d Ubuntu-24.04 -- bash -lc "~/code/riftbound-trainer/scripts/train_night.sh"`
4. En las propiedades de la tarea, marca "Ejecutar tanto si el usuario inició
   sesión como si no" y, en Condiciones, "Activar el equipo para ejecutar esta
   tarea".

El script se corta solo a las 6 horas, guarda el modelo y escribe un reporte.

Si la PC usa Ubuntu directamente, sin Windows, prográmalo con cron:

```bash
bash ~/code/alpharune/scripts/training/install_nightly.sh        # 1:00 a. m.
bash ~/code/alpharune/scripts/training/install_nightly.sh 23 30  # otra hora
bash ~/code/alpharune/scripts/training/install_nightly.sh --remove
```

En WSL2, cron solo corre mientras Ubuntu está abierto, por eso ahí conviene
el Programador de tareas de Windows.

## Paso 4: revisar cada mañana

Abre `~/code/riftbound-trainer/reports/latest.md`. Lo importante:

| Línea del reporte | Qué significa | Qué esperar |
|---|---|---|
| Win rate vs random | Partidas ganadas contra el jugador aleatorio | Sube rápido, a más de 90% |
| Win rate vs mcts20 | Partidas ganadas contra la búsqueda MCTS | El número que realmente importa. Sube lento |
| Win rate vs anterior | Contra el modelo de la noche anterior | Por encima de 50% si mejoró |
| Pérdidas (loss) | Señales técnicas del entrenamiento | Que no exploten ni den `nan` |

Si un día algo se ve raro, abre Claude Code en esa carpeta y dile "lee
`reports/` y los logs de anoche, y dime qué pasó".

## El prompt para Claude Code

Copia todo el bloque.

```text
Quiero entrenar un agente que juegue Riftbound (un juego de cartas), usando el motor C++ que está en ~/code/alpharune (rama radiance). Trabajo en WSL2 Ubuntu 24.04 sobre Windows. Tengo una RTX 4060 Ti de 8 GB y 32 GB de RAM. No sé nada de machine learning, así que explícame en español, en simple, cada decisión importante, y pregúntame antes de instalar cosas grandes o de borrar algo.

Contexto del motor (léelo antes de empezar):
- ~/code/alpharune/CLAUDE.md: cómo compilar y las convenciones del motor.
- ~/code/alpharune/src/openspiel/riftbound_game.{h,cpp} y riftbound_state.{h,cpp}: el juego implementa la interfaz de OpenSpiel (LegalActions, CurrentPlayer, IsTerminal, Returns, ObservationTensor por jugador con la información oculta enmascarada, ApplyAction). El juego se crea con los parámetros deck1, deck2 (rutas a listas .txt de ~/code/alpharune/decks/) y seed.
- ~/code/alpharune/src/openspiel/action_vocab.h: el espacio de acciones (NumDistinctActions), unas 24 mil acciones; en cada estado solo unas pocas son legales.
- ~/code/alpharune/src/ml/feature_extractor.h: el tamaño de la observación (kStateFeatureDim = 10688).
- ~/code/alpharune/src/openspiel/openspiel_match.cpp: cómo se juegan partidas contra los agentes de referencia (random, mcts:sims=N).
- El motor no trae bindings de Python ni dependencia de torch, y así debe seguir: todo lo de entrenamiento va en un repo aparte.

Lo que quiero que construyas, en un repo nuevo ~/code/riftbound-trainer (git init, sin push):

1. Dependencias: instala lo necesario para compilar el motor (build-essential, cmake, ninja-build, libboost-all-dev, nlohmann-json3-dev, python3-venv, python3-dev). Compila el motor siguiendo su CLAUDE.md y corre sus tests para confirmar que todo pasa. Crea un venv de Python con PyTorch con soporte CUDA y verifica que torch.cuda.is_available() sea True y que vea la RTX 4060 Ti.

2. Bindings: un módulo de Python con pybind11 que enlace contra la librería del motor (libriftbound_core y la parte de OpenSpiel que use RiftboundGame/RiftboundState), compilado desde riftbound-trainer con CMake sin modificar el repo del motor. Debe permitir: crear un juego con dos mazos y una semilla, obtener el jugador actual, las acciones legales, la observación de un jugador como arreglo de numpy, aplicar una acción, saber si terminó y los resultados. Para velocidad, agrega una forma de correr muchas partidas a la vez (un "vector" de N juegos) con una sola llamada por paso. Incluye un agente aleatorio y otro MCTS del lado C++ para evaluar. Escribe tests en Python y una prueba que mida cuántos pasos por segundo logra.

3. El agente: una red en PyTorch (MLP) que recibe la observación de 10688 números y devuelve logits para las acciones (aplica una máscara para que solo las legales tengan probabilidad) y un valor (quién va ganando). Debe caber cómodo en 8 GB.

4. Entrenamiento: auto-juego con PPO, con máscara de acciones legales. El oponente se elige de un grupo con el modelo actual, modelos anteriores guardados y, de vez en cuando, el agente aleatorio, para que no se especialice contra sí mismo. Los mazos de cada partida se eligen al azar entre los de ~/code/alpharune/decks/. Guarda checkpoints con frecuencia y permite retomar exactamente donde quedó. Registra métricas con TensorBoard.

5. Evaluación: un script que juegue el modelo contra random, contra mcts:sims=20 y contra el checkpoint anterior (unas 200 partidas cada uno, alternando quién empieza) y escriba reports/YYYY-MM-DD.md y reports/latest.md con los porcentajes de victoria e intervalos de confianza, más un resumen en español de una línea sobre si mejoró.

6. Prueba corta: corre 10 minutos de entrenamiento y la evaluación, y muéstrame el reporte. Confirma que retomar desde el checkpoint funciona.

7. Corrida nocturna: un script scripts/train_night.sh que active el venv, actualice y recompile el motor solo si cambió (git pull en ~/code/alpharune), entrene con un límite de 6 horas guardando antes de cortar, corra la evaluación y deje el reporte. Debe escribir un log por noche en logs/ y no fallar si la noche anterior se cortó a medias. Explícame cómo programarlo en el Programador de tareas de Windows con wsl.exe.

8. Un README en español con: cómo funciona todo explicado en simple, cómo leer el reporte, qué hacer si algo falla, y cómo jugar contra el agente entrenado (por ejemplo, un modo que juegue partidas modelo vs mcts e imprima los resultados).

Reglas: haz commits en riftbound-trainer a medida que cada parte funcione. No modifiques ~/code/alpharune salvo que sea imprescindible, y si lo es, pregúntame primero. Si algo del motor no está claro, léelo en el código antes de suponer. Si el entrenamiento no aprende (el win rate contra random no sube después de la prueba), investiga y explícame la causa antes de seguir.
```

## Si más adelante quieres más

- **Más fuerza:** cuando le gane a `mcts20`, cambia la evaluación a `mcts50` o
  `mcts200`.
- **Tus mazos:** pon tus listas en una carpeta y pídele a Claude Code que
  entrene solo con ellas, para un agente especializado en tu meta.
- **Sets nuevos:** el vocabulario de cartas del motor tiene espacio hasta la
  carta 2000. Cuando entren cartas nuevas sigue funcionando. Solo hay que
  entrenar un poco más para que el agente aprenda a usarlas.
