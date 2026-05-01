# Tetris Model Simulator
This project implements two models (heuristic and greedy) of tetris playing agents, runs them on a randomly generated, fixed-size sequence of tetronimos. Tetronimos are one of four manually defined shapes.

---
### `tetris_model.c`

Simulates two Tetris agents and prints analysis of behavior.
#### Compile
```bash
cc -O2 -Wall -o tetris_model tetris_model.c
```
#### Usage
```bash
./tetris_model
```

#### Outputs: `greedy_log.txt`, `heuristic_log.txt`

---

### `tetris_display.c`:

Reads a log produced by tetris_model and replays each turn in the terminal with a 0.5-second pause between frames.

#### Compile
```bash
cc -O2 -Wall -o tetris_display tetris_display.c
```
#### Usage
```bash
./tetris_display <log_file.txt>
```