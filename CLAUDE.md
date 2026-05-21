# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires `clang` and GNU `make`. The build has a pre-compilation step that generates `src/generated_data.c` (precomputed bitboard lookup tables and Zobrist hashes).

```bash
make           # release build → build/pawnstar
make DEBUG=1   # debug build with ASan + UBSan (-Og)
make clean     # remove build artifacts
```

Compiler flags include `-mbmi2` (BMI2 intrinsics required), `-std=c17`, `-I include`, and `-D _POSIX_C_SOURCE=200809L`. Debug builds add `-fsanitize=undefined,address`.

## Running Tests

Tests are embedded as engine commands. Build first, then:

```bash
./build/pawnstar
perft          # move generation regression suite (standard PERFT positions)
perft x        # same, with cross-validation against an independent pseudo-legal generator
seetests       # static exchange evaluation tests
postests 9     # Bratko-Kopec search positions at depth 9 (default depth is 9)
```

There is no separate test binary or unit test framework.

## Code Formatting

clang-format with Microsoft base style, 120-column limit. Config is in `.clang-format`.

## Documentation

```bash
doxygen .      # generates HTML docs; Doxyfile is in the repo root
```

All public types and functions have Doxygen comments (`/// @brief`, `///<`).

## Architecture

Pawnstar is a UCI chess engine written in **C17** using bitboard board representation and alpha-beta search.

**Naming conventions** — types are `lower_snake_case_t`, functions are `type_verb` or `type_verb_noun` (e.g. `position_make_move`, `move_list_push_back`), constants and macros are `UPPER_SNAKE_CASE`. One logical module per `.h`/`.c` pair; inline functions live in the header.

**Board representation** — `position_t` ([include/position.h](include/position.h)) holds six per-piece bitboards, two per-color bitboards, a `squares[64]` array for O(1) piece lookup, Zobrist hash, castling rights, and en-passant square. `bitboard_t` is a `typedef uint64_t` in LERF mapping (bit 0 = a1, bit 63 = h8). The `BB_FOREACH` macro iterates set bits LSB-first.

**Move encoding** — `move_t` is a `typedef int64_t` packing from/to squares (bits 0–11), moving piece (12–14), captured piece (15–17), move type flags (18–21), gives-check flag (22), and sort score (32–63). `move_list_t` and `variation_t` are fixed-capacity stack-allocated arrays — no heap allocation during search.

**Attack generation** — `bishop_attacks` and `rook_attacks` ([include/attacks.h](include/attacks.h)) use BMI2 `_pext_u64` with precomputed occupancy masks. The lookup tables live in `src/generated_data.c`. `Pins` ([include/pins.h](include/pins.h)) computes pin rays and absolute-pin masks before move generation.

**Search** — `game_t` ([include/game.h](include/game.h)) owns all search state: two transposition tables (64 MB main, 8 MB quiescence), a history heuristic table, opening book, chess clock, and position history stack for repetition detection. Search runs on a `thrd_t` worker thread so UCI `stop` can interrupt it.

The search stack ([src/search_root_node.c](src/search_root_node.c), [src/search_alphabeta.c](src/search_alphabeta.c), [src/search_quiescent.c](src/search_quiescent.c)) uses iterative deepening from depth 3, PVS (principal variation search), null-move pruning, late-move reduction (LMR after the 3rd move at depth > 2 in non-PV nodes), and check/promotion/en-passant extensions. Move ordering: TT move first, then MVV-LVA combined with the history heuristic.

**Evaluation** — `evaluate_position` ([src/evaluation.c](src/evaluation.c)) scores material, piece-square tables (tapered between opening and endgame phases), pawn structure (passed, isolated, backward, doubled, defended), mobility, and king safety (pawn shelter, attacking pieces near the king).

**Opening book** — loaded from a plain-text file; positions are sorted by Zobrist hash at load time; runtime lookup is O(log n) via `bsearch`. Move selection uses a xorshift64 PRNG.

**Generated data** — [generate_constants/](generate_constants/) is a standalone C program that writes [src/generated_data.c](src/generated_data.c). Its output is committed; regenerate only when attack-mask logic changes (`make gen` builds the generator).

**UCI diagnostics** — beyond the standard UCI command set, the engine accepts `eval`, `getboard`, `bookmoves`, `freebook`, `dbg`, `dbgclear`, and `help`. See [src/input_handling.c](src/input_handling.c).
