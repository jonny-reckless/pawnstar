# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires `clang` and GNU `make`. The build has a pre-compilation step that generates `src/generated_data.c` (precomputed bitboard lookup tables and Zobrist hashes).

```bash
make           # release build → build/pawnstar
make DEBUG=1   # debug build with ASan + UBSan (-O0)
make clean     # remove all build artifacts (including build-test/)
```

Compiler flags include `-mbmi2` (BMI2 intrinsics required), `-std=c17`, `-I src`, and `-D _POSIX_C_SOURCE=200809L`. Debug builds add `-fsanitize=undefined,address`.

## Running Tests

```bash
make check     # build and run all three test suites (perft at D5)
make test      # build test binaries only, do not run
```

Run individually after `make test`:

```bash
./build-test/test_perft          # 649 perft node-count cases (D1–D5 by default)
./build-test/test_perft 4        # limit to D4
./build-test/test_bratko_kopec   # 24 Bratko-Kopec search positions
./build-test/test_bratko_kopec 7 # limit to depth 7
./build-test/test_see            # static exchange evaluation
```

Tests are compiled directly by the Makefile (no CMake) into `build-test/`, and linked against `build/libpawnstar.a`. The `DEBUG=1` flag applies uniformly to both the library and the test executables.

## Code Formatting

clang-format with Microsoft base style, 120-column limit. Config is in `.clang-format`.

## Architecture

Pawnstar is a UCI chess engine written in **C17** using bitboard board representation and alpha-beta search.

**Naming conventions** — types are `lower_snake_case_t`, functions are `type_verb` or `type_verb_noun` (e.g. `position_make_move`, `move_list_push_back`), constants and macros are `UPPER_SNAKE_CASE`. One logical module per `.h`/`.c` pair; inline functions live in the header.

**Board representation** — `position_t` ([src/position.h](src/position.h)) is a fully self-contained, 160-byte struct. It holds six per-piece bitboards, two per-color bitboards, `squares[64]` for O(1) piece lookup, `scores[2]` (incremental material + piece-square totals), Zobrist `hash`, `checkers` bitboard, `castling_rights`, `en_passant_square`, `half_move_clock`, and `move_counter` — all as direct flat fields (no nested state struct). Fields are ordered largest-alignment-first to eliminate padding; the first 64 bytes match `see_board_t` for the SEE `memcpy`. `bitboard_t` is `typedef uint64_t` in LERF mapping (bit 0 = a1, bit 63 = h8).

**Copy-make move** — `position_make_move(dst, src, move)` copies `src` into `dst` and applies the move. Undo is a free pointer decrement. `game_t` owns a `position_t positions[256]` stack and a `position_t *position` pointer to the top. `game_play_move` / `game_undo_move` / `game_make_null_move` increment/decrement this pointer. During search, `search_state_t` maintains its own copy-make `pos_stack` and a separate compact `hash_stack` for repetition detection.

**Move encoding** — `move_t` is a `typedef int64_t` packing from/to squares (bits 0–11), moving piece (12–14), captured piece (15–17), move type flags (18–21), gives-check flag (22), and sort score (32–63). `move_list_t` and `variation_t` are fixed-capacity stack-allocated arrays — no heap allocation during search.

**Attack generation** — `bishop_attacks` and `rook_attacks` ([src/attacks.h](src/attacks.h)) use BMI2 `_pext_u64` with precomputed occupancy masks. The lookup tables live in `src/generated_data.c`. `pins_t` ([src/pins.h](src/pins.h)) computes pin rays and absolute-pin masks before move generation.

**Search** — `game_t` ([src/game.h](src/game.h)) owns all shared search infrastructure: transposition table (64 MB, 256-stripe mutex), history heuristic table (`_Atomic uint32_t counts[MAX_PLY × 4096]`), opening book, chess clock, position stack, cancel flag (`atomic_bool is_cancel_pending`), and `n_helpers` (physical CPUs − 1). Search runs on a background `thrd_t` so UCI `stop` can interrupt it.

`search_state_t` ([src/search_state.h](src/search_state.h)) is per-thread search state: a copy-make `pos_stack` (seeded from the game's current position at thread creation), a compact `hash_stack` of `{hash, half_move_clock}` entries for repetition detection (16 bytes × n vs 160 bytes × n for full positions), a `node_count`, and a pointer to the shared `game_t`. Allocated with `malloc` per thread; freed when the thread exits.

The search stack ([src/search_root_node.c](src/search_root_node.c), [src/search_alphabeta.c](src/search_alphabeta.c), [src/search_quiescent.c](src/search_quiescent.c)) uses iterative deepening from depth 3, PVS (principal variation search), null-move pruning (R=4, guarded by: null-window, not a null-move position, not in check, not in endgame, PST score ≥ beta+100), late-move reduction (LMR: −1 ply from move index >3 when depth>2, null-window, non-capture, non-pawn; −2 from move index >6; −3 from move index >6 with zero history count), and check/promotion extensions. Move ordering: TT move first, then MVV-LVA (victim×1000 − attacker×100) combined with history heuristic.

**Lazy SMP** — At the start of each `search_root_node` call, `n_helpers` threads are spawned via `thrd_create`. Each helper calls `search_state_from_game` and independently runs the full iterative-deepening loop from START_DEPTH until `is_cancel_pending` is set. Parallelism is entirely implicit: threads share only the transposition table, and natural TT races cause them to explore slightly different subtrees. When the main thread's search finishes or times out, it stores `is_cancel_pending = true` and `thrd_join`s all helpers before returning.

**Evaluation** — Incremental material + PST scores are kept in `position_t::scores[WHITE]` and `position_t::scores[BLACK]`, updated by `position_add/remove/move_piece`. `evaluate_position` ([src/evaluation.c](src/evaluation.c)) reads these for a lazy-evaluation cutoff (±200 cp), then computes bishop-pair bonus, pawn structure (passed, isolated, backward, doubled, defended), mobility, and king safety.

**Static exchange evaluation** — `evaluate_static_exchange` ([src/static_exchange_evaluation.c](src/static_exchange_evaluation.c)) makes the move into a `position_t` on the stack, then `memcpy`s the first 64 bytes into a `see_board_t` (pawns through black_pieces). The swap-off loop uses the cheapest available attacker at each step.

**Opening book** — loaded from a plain-text file; move selection uses a xorshift64 PRNG.

**Generated data** — [generate_constants/](generate_constants/) is a standalone C program that writes [src/generated_data.c](src/generated_data.c). Output is committed; regenerate only when attack-mask logic changes (`make gen`).

**UCI diagnostics** — beyond standard UCI, the engine accepts `eval`, `getboard`, `bookmoves`, `freebook`, `dbg`, `dbgclear`, and `help`. See [src/input_handling.c](src/input_handling.c).
