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

Three standalone test executables live in `tests/`. Build and run all of them with:

```bash
make check
```

`make test` builds without running; `make check` builds then runs. The Makefile compiles each executable directly (no CMake, no external test framework) into `build-test/`:

| Executable | Coverage | Cases |
|---|---|---|
| `test_perft` | Move-generation node counts (D1–D5) | 649 |
| `test_bratko_kopec` | Bratko-Kopec search positions — verifies best move | 24 |
| `test_see` | Static exchange evaluation | 2 |

The executables can also be run individually after `make test` has built them:

```bash
./build-test/test_perft          # all depths up to D5 (649 cases)
./build-test/test_perft 4        # limit to D4
./build-test/test_bratko_kopec   # full depth per case
./build-test/test_bratko_kopec 7 # limit to depth 7
./build-test/test_see
```

---

## Architecture

### Board representation

`position_t` ([src/position.h](src/position.h)) is the central state struct. It holds six per-piece bitboards (`pawns`, `knights`, …, `kings`), two per-color bitboards, a `squares[64]` array for O(1) piece lookup, `scores[2]` — incrementally maintained material + piece-square totals for each side — and the Zobrist hash, checker bitboard, castling rights, en-passant square, and half/full-move counters, all as direct fields.

Move application uses **copy-make**: `position_make_move(dst, src, move)` copies `src` into `dst` and applies the move to `dst`. Undo is a free pointer decrement. `game_t` maintains a stack of `position_t` objects for the current game line.

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

`bishop_attacks` and `rook_attacks` ([src/attacks.h](src/attacks.h)) use BMI2 `_pext_u64` with precomputed occupancy masks and shared attack sets to compute sliding-piece attacks in two instructions. The lookup tables live in `src/generated_data.c`. A classical ray-subtraction fallback is available for non-BMI2 builds (`USE_PEXT_BITBOARDS=0`).

Pin detection ([src/pins.h](src/pins.h)) computes pin rays and absolute-pin masks before move generation, allowing the legal-move generator to filter pinned pieces efficiently.

### Search

`game_t` ([src/game.h](src/game.h)) owns all shared search infrastructure:

- Transposition table (64 MB, 24-byte entries, direct-mapped by `hash % size`; access serialized by 256 stripe mutexes indexed `table_index & 255`)
- History heuristic table: `_Atomic uint32_t counts[MAX_PLY × 4096]`, indexed by `(ply, from+to)`, accumulated with relaxed atomics so Lazy SMP helper threads update it safely
- Opening book indexed by Zobrist hash
- Chess clock and time-control parameters
- Position history stack (`position_t positions[256]`) for the current game line
- `atomic_bool is_cancel_pending` — set by UCI `stop` or time expiry; polled by all search threads
- `n_helpers` — number of Lazy SMP helper threads (physical CPUs − 1)

Search runs on a background `thrd_t` so the UCI `stop` command can interrupt it without blocking.

`search_state_t` ([src/search_state.h](src/search_state.h)) is per-thread state: a copy-make `pos_stack` seeded from the game's current position at thread creation, a compact `hash_stack` of `{hash, half_move_clock}` entries for repetition detection (16 bytes × n vs 160 bytes × n for full positions), a node counter, and a shared pointer to `game_t`. Allocated with `malloc`; freed after the thread's search completes.

The search ([src/search_root_node.c](src/search_root_node.c), [src/search_alphabeta.c](src/search_alphabeta.c), [src/search_quiescent.c](src/search_quiescent.c)):

- **Iterative deepening** from START_DEPTH (3) up to MAX_PLY. A shallow full-width pass at START_DEPTH scores each root move before the first real iteration, improving move ordering for all subsequent depths.
- **Time management** (standard clock only): allocates `ms_remaining / moves_to_go` per move; hard stop at 2× that allocation or the remaining budget minus 1 s. Early exit when the best move has been consistent across all completed depths and the score is stable within SCORE_INSTABILITY_THRESHOLD.
- **PVS** (principal variation search): at PV nodes (beta > alpha+1), moves after the first are searched with a null window; only if the scout raises alpha is a full-window re-search issued.
- **Null-move pruning** (R=4): at null-window nodes, when the side to move is not in check, the position was not reached by a null move, the position is not in the endgame, and the PST score exceeds beta+100, a null move is tried. If the reduced search still exceeds beta, the node is pruned.
- **Late-move reduction (LMR)**: at null-window nodes with depth > 2, quiet non-pawn moves are reduced. Move index > 3 (4th+ move): −1 ply. Move index > 6 (7th+ move): −2 ply. Move index > 6 with zero history count: −3 ply. A move that raises alpha after reduction is re-searched at full depth.
- **Check extension**: +1 ply when the side to move is in check.
- **Promotion extension**: +1 ply on pawn-promotion moves.
- **Quiescence search** on captures only; positions in check fall back to a full `search()` call so every legal reply is considered. Standing-pat (static evaluation as a lower bound) is applied before generating captures.
- **Lazy SMP**: at the start of each search, `n_helpers` background threads are spawned (one per physical CPU minus one for the main thread). Each helper independently runs the same iterative-deepening loop from the root, sharing only the transposition table. TT races between threads cause natural divergence in move ordering; the helper threads populate TT entries that the main thread exploits for better pruning. When the main thread finishes or is cancelled, it sets `is_cancel_pending` and joins all helpers.

Move ordering: transposition-table move first (searched serially before the full move list is generated), then MVV-LVA (victim value × 1000 − attacker value × 100) combined with the history heuristic count.

### Evaluation

Material and piece-square scores are maintained incrementally in `position_t::scores[2]`, updated on every `position_add/remove/move_piece` call; undo is free under copy-make. `evaluate_position` ([src/evaluation.h](src/evaluation.h), [src/evaluation.c](src/evaluation.c)) reads these cached scores for a lazy-evaluation cutoff (±200 cp of the window), then falls through to compute:

- **Bishop-pair bonus**: +30 cp when a side has bishops on both light and dark squares.
- **Pawn structure** (per pawn): passed pawn bonus scaled by rank (up to +30 cp on rank 7); defended pawn +5 cp; backward pawn −10 cp; doubled pawn −10 cp; isolated pawn −20 cp. Passed pawns that are also doubled are excluded from the bonus.
- **Mobility**: bishop and rook attacks counted with occupied-square masking. Attack counts are looked up in `BISHOP_ATTACK_SCORES[14]` and `ROOK_ATTACK_SCORES[15]` (negative for low mobility, up to +20 cp for high mobility). Bishop attacks onto squares defended by enemy pawns are excluded.
- **King safety**: pawn-shelter check up to three ranks ahead (±15/10/5 cp per pawn in each rank); open-file penalties (−15 cp if the king's file has no friendly pawns, −10 cp per open adjacent file); adjacent-piece bonus (+10 cp per friendly non-pawn piece in king ring-1, +5 cp in ring-2); PST switches from `KING_SQUARE_MIDGAME` (castled-corner preference) to `KING_SQUARE_ENDGAME` (centralisation) based on endgame detection. Pawn-shelter and open-file penalties are only applied in the middlegame.

Piece values: P=100, N=300, B=300, R=500, Q=900.

Static exchange evaluation ([src/static_exchange_evaluation.h](src/static_exchange_evaluation.h)) estimates the material outcome of a capture sequence for move ordering.

### Opening book

The opening book is loaded from a plain-text file (one line of play per line, moves in UCI notation). Move selection is randomised with a xorshift64 PRNG seeded from program start time.

---

## Code conventions

- **Types**: `lower_snake_case_t` (e.g. `position_t`, `move_t`, `bitboard_t`)
- **Functions**: `type_verb` or `type_verb_noun` (e.g. `position_make_move`, `move_list_push_back`)
- **Constants and macros**: `UPPER_SNAKE_CASE` (e.g. `MAX_PLY`, `PIECE_VALUES`, `BB_FOREACH`)
- **Files**: one logical module per `.h`/`.c` pair in `src/`; inline functions live in the header
- All headers are self-contained (`#pragma once`, explicit includes)
- All control-flow bodies use braces, enforced by `InsertBraces: true` in `.clang-format`
- Doxygen comments (`/// @brief`, `///<`) on all public types and functions

---

## Documentation

Generate Doxygen HTML documentation from this directory:

```bash
doxygen .
```
