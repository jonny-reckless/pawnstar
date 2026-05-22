# Pawnstar

A UCI chess engine written in C17, using bitboards for board representation and alpha-beta search with iterative deepening.

Move generation runs at approximately **270 million positions per second** on a modern laptop (BMI2 PEXT required).

---

## Requirements

- `clang` (C17)
- GNU `make`
- A CPU with BMI2 support (`-mbmi2`; Intel Haswell / AMD Zen 2 or later)

---

## Building

```bash
make           # release build  → build/pawnstar
make DEBUG=1   # debug build with ASan + UBSan
make clean     # remove all build artefacts
```

The build has one code-generation step: `generate_constants/generate_constants.c` is compiled and run to produce `src/generated_data.c` (precomputed bitboard lookup tables and Zobrist hashes).

---

## Running

```
./build/pawnstar
```

Pawnstar speaks the UCI protocol. Connect it to any UCI-compatible GUI (Arena, Cute Chess, etc.) or communicate with it directly on stdin/stdout.

### Commands

| Command | Description |
|---------|-------------|
| `uci` | Enter UCI mode; prints engine identity and options |
| `ucinewgame` | Reset state for a new game |
| `position` | Set the position and series of moves |
| `go` | Search the current position and output `bestmove` |
| `stop` | Stop searching and return the best move found so far |
| `isready` | Respond with `readyok` |
| `eval` | Display the static evaluation of the current position |
| `getboard` | Display the FEN string for the current position |
| `bookmoves` | List available opening-book moves for the current position |
| `freebook` | Release the opening book from memory |
| `dbg` | Print internal diagnostic counters |
| `dbgclear` | Reset diagnostic counters |
| `help` | List all commands |
| `quit` | Exit |

### Tests

Tests use Google Test and are built separately:

```bash
make test
```

This configures a CMake build in `build-test/`, compiles three test executables, and runs them via `ctest`. The suites are:

| Executable | Coverage |
|---|---|
| `test_perft` | Move-generation node counts (D1–D6, 778 cases) |
| `test_bratko_kopec` | Bratko-Kopec search positions — verifies best move |
| `test_see` | Static exchange evaluation |

---

## Architecture

### Board representation

`position_t` ([include/position.h](include/position.h)) is the central state struct. It holds six per-piece bitboards (`pawns`, `knights`, …, `kings`), two per-color bitboards, a `squares[64]` array for O(1) piece lookup, Zobrist hash, castling rights, en-passant square, and half/full-move counters.

`bitboard_t` is a `typedef uint64_t` in LERF mapping: bit 0 = a1, bit 63 = h8. Direct bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`) are used throughout; the `BB_FOREACH` macro iterates set bits from LSB to MSB.

### Move encoding

`move_t` is a `typedef int64_t`. One 64-bit word packs:

| Bits | Field |
|------|-------|
| 0–5 | Destination square |
| 6–11 | Origin square |
| 12–14 | Moving piece (`piece_t`) |
| 15–17 | Captured piece (`piece_t`) |
| 18–21 | Move type (`move_type_t`) |
| 22 | Gives-check flag |
| 32–63 | Sort score (signed, used by move ordering) |

`move_list_t` and `variation_t` are fixed-capacity stack-allocated arrays — no heap allocation during search.

### Attack generation

`bishop_attacks` and `rook_attacks` ([include/attacks.h](include/attacks.h)) use BMI2 `_pext_u64` with precomputed occupancy masks and shared attack sets to compute sliding-piece attacks in two instructions. The lookup tables live in `src/generated_data.c`. A classical ray-subtraction fallback is available for non-BMI2 builds (`USE_PEXT_BITBOARDS=0`).

Pin detection ([include/pins.h](include/pins.h)) computes pin rays and absolute-pin masks before move generation, allowing the legal-move generator to filter pinned pieces efficiently.

### Search

`game_t` ([include/game.h](include/game.h)) owns all search state:

- A single transposition table (64 MB)
- A history heuristic table indexed by `(ply, from+to)`
- An opening book of moves indexed by Zobrist hash
- A chess clock
- A position history stack for repetition detection

Search runs on a background `thrd_t` worker thread so the UCI `stop` command can interrupt it immediately.

The search stack ([src/search_root_node.c](src/search_root_node.c), [src/search_alphabeta.c](src/search_alphabeta.c), [src/search_quiescent.c](src/search_quiescent.c)):

- Iterative deepening from depth 3
- Alpha-beta with PVS (principal variation search)
- Null-move pruning
- Late-move reduction (LMR): reduces depth for moves after the 3rd at depth > 2 in non-PV nodes
- Check, promotion, and en-passant extensions
- Quiescence search on captures only

Move ordering: transposition-table move first, then MVV-LVA (victim value × 10000 − attacker value × 1000) combined with the history heuristic count.

### Evaluation

`evaluate_position` ([include/evaluation.h](include/evaluation.h), [src/evaluation.c](src/evaluation.c)) scores:

- Material count
- Piece-square tables (tapered between opening/middlegame and endgame phases)
- Pawn structure: passed, isolated, backward, doubled, and defended pawns
- Mobility
- King safety (pawn shelter, attacking pieces near the king)

Static exchange evaluation ([include/static_exchange_evaluation.h](include/static_exchange_evaluation.h)) estimates the material outcome of a capture sequence for move ordering.

### Opening book

The opening book is loaded from a plain-text file (one line of play per line, moves in UCI notation). Move selection is randomised with a xorshift64 PRNG seeded from program start time.

---

## Code conventions

- **Types**: `lower_snake_case_t` (e.g. `position_t`, `move_t`, `bitboard_t`)
- **Functions**: `type_verb` or `type_verb_noun` (e.g. `position_make_move`, `move_list_push_back`)
- **Constants and macros**: `UPPER_SNAKE_CASE` (e.g. `MAX_PLY`, `PIECE_VALUES`, `BB_FOREACH`)
- **Files**: one logical module per `.h`/`.c` pair; inline functions live in the header
- All headers are self-contained (`#pragma once`, explicit includes)
- Doxygen comments (`/// @brief`, `///<`) on all public types and functions

---

## Documentation

Generate Doxygen HTML documentation from this directory:

```bash
doxygen .
```
