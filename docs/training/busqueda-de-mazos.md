# Buscar una mejor lista contra un mazo fijo

`scripts/deck_search/deck_search.py` parte de tu lista y prueba cambiar una
carta a la vez. Simula cada variante contra un rival fijo y se queda con la
que gana claramente más. No usa la GPU ni tokens de Claude: es CPU de tu PC,
por eso conviene dejarlo corriendo de noche.

## Cómo funciona

1. **Mide tu lista** con 400 partidas contra el rival.
2. **Cada ronda prueba 24 cambios** de una copia: sale una carta de tu main
   deck y entra otra legal para tu Legend. Primero prueba tu sideboard,
   después las cartas que aparecen en los otros mazos del repo y después el
   resto de las cartas legales.
3. **Descarta rápido lo malo:** cada cambio juega 40 partidas, el mejor cuarto
   juega 80 más, y así hasta quedar uno.
4. **Confirma con partidas nuevas:** el ganador de la ronda juega 400 partidas
   nuevas. Solo se acepta si le gana a tu lista actual con 95% de confianza, y
   otra vez después de 400 partidas más. Así evita quedarse con cambios que
   tuvieron suerte.
5. **Repite** desde la lista mejorada hasta que se acaba el tiempo. Si lo
   cortas, retoma donde quedó.

Una ronda tarda unos 50 minutos en una PC de 10 núcleos, así que una noche
alcanza para 8 o 9 rondas. Si tu PC tiene más núcleos, va proporcionalmente
más rápido.

## Correrlo

Desde la carpeta del motor (`~/code/alpharune` en la PC de la guía de
entrenamiento), con el motor ya compilado:

```bash
git pull
bash scripts/deck_search/run_night.sh decks/draven_nuevo.txt decks/riven_radiance_namdang.txt 7
```

El último número son las horas. Para que siga aunque cierres la terminal:

```bash
nohup bash scripts/deck_search/run_night.sh decks/draven_nuevo.txt decks/riven_radiance_namdang.txt 7 &
```

Para programarlo cada noche, usa el Programador de tareas de Windows igual
que en la guía de entrenamiento, con este argumento:

```text
-d Ubuntu-24.04 -- bash -lc "cd ~/code/alpharune && scripts/deck_search/run_night.sh decks/draven_nuevo.txt decks/riven_radiance_namdang.txt 7"
```

No corras la búsqueda y el entrenamiento la misma noche: los dos compiten por
el procesador. Alterna noches.

## Leer el resultado

Abre `deck_search_runs/draven_nuevo_vs_riven_radiance_namdang/report.md`:

- **Win rate de tu lista original y de la mejor encontrada.**
- **Cambios aceptados**, con los números que los justificaron.
- **La mejor lista**, lista para copiar. También queda en `best_deck.txt` del
  mismo folder, y la puedes simular desde el MCP.

Si después de varias noches no acepta ningún cambio, tu lista ya está cerca de
lo mejor que esta búsqueda puede encontrar con cambios de una carta.

## Límites

- **Los bots no son personas.** Los bots MCTS ven la mano rival y juegan peor
  que un humano bueno. Una lista que les gana es una pista fuerte, no una
  garantía contra el jugador real.
- **Cambios de una carta.** La búsqueda no prueba reemplazar varias cartas a la
  vez ni cambiar battlefields o runas.
- **Opciones útiles:** `--agent mcts:sims=50` usa bots más fuertes pero más
  lentos; `--variants` y `--confirm-games` ajustan cuánto explora y cuánta
  confianza pide. `python3 scripts/deck_search/deck_search.py --help` las lista
  todas.
