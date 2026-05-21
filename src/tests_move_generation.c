/// @file Standard perft move generation tests.

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "attacks.h"
#include "position.h"

typedef struct
{
    const char *position;
    int         depth;
    uint64_t    count;
} perft_test_t;

// ---------------------------------------------------------------------------
// Generated perft test data
// ---------------------------------------------------------------------------

extern const char *perft_results[132];

// ---------------------------------------------------------------------------
// perft search
// ---------------------------------------------------------------------------

static void perft(position_t *position, int depth, uint64_t *num_moves)
{
    move_list_t move_list = position_generate_legal_moves(position);
    if (depth > 1)
    {
        for (int i = 0; i < move_list.size; ++i)
        {
            move_undo_t undo;
            position_make_move(position, move_list.items[i], &undo);
            perft(position, depth - 1, num_moves);
            position_undo_move(position, move_list.items[i], &undo);
        }
        return;
    }
    *num_moves += (uint64_t)move_list.size;
}

// ---------------------------------------------------------------------------
// Pseudo-legal move generator (regression check only)
// ---------------------------------------------------------------------------

static const uint8_t DOUBLE_PUSH_RANK[2] = {3, 4};
static const uint8_t PROMOTION_RANK[2]   = {7, 0};

static move_list_t generate_pseudo_legal_moves(const position_t *position)
{
    move_list_t moves;
    moves.size                        = 0;
    const color_t    color            = position_color_to_move(position);
    const bitboard_t friendly_pieces  = position_pieces_of_color(position, color);
    const bitboard_t occupied_squares = position_occupied_squares(position);
    const bitboard_t vacant_squares   = position_vacant_squares(position);
    const bitboard_t enemy_pieces     = (occupied_squares & (~friendly_pieces));

    // Pawn moves.
    bitboard_t pawns = (position->pawns & friendly_pieces);
    square_t   from;
    BB_FOREACH(from, pawns)
    {
        const int from_val = from;
        square_t  to;
        to = color == WHITE ? (square_t)(from_val + 8) : (square_t)(from_val - 8);
        if (!((bitboard_from_square(to) & occupied_squares)))
        {
            if (square_rank(to) == PROMOTION_RANK[color])
            {
                move_list_push_back(&moves, move_promotion_quiet(from, to, QUEEN));
                move_list_push_back(&moves, move_promotion_quiet(from, to, ROOK));
                move_list_push_back(&moves, move_promotion_quiet(from, to, BISHOP));
                move_list_push_back(&moves, move_promotion_quiet(from, to, KNIGHT));
            }
            else
            {
                move_list_push_back(&moves, move_non_capture(from, to, PAWN));
                square_t to2 = color == WHITE ? (square_t)(from_val + 16) : (square_t)(from_val - 16);
                if (square_rank(to2) == DOUBLE_PUSH_RANK[color] && !((bitboard_from_square(to2) & occupied_squares)))
                {
                    move_list_push_back(&moves, move_double_push(from, to2));
                }
            }
        }
        const bitboard_t pawn_attacks = color == WHITE ? PAWN_ATTACKS_WHITE[from] : PAWN_ATTACKS_BLACK[from];
        bitboard_t       captures     = (pawn_attacks & enemy_pieces);
        square_t         cap_sq;
        BB_FOREACH(cap_sq, captures)
        {
            if (square_rank(cap_sq) == PROMOTION_RANK[color])
            {
                move_list_push_back(&moves, move_promotion(from, cap_sq, QUEEN, position->squares[cap_sq]));
                move_list_push_back(&moves, move_promotion(from, cap_sq, ROOK, position->squares[cap_sq]));
                move_list_push_back(&moves, move_promotion(from, cap_sq, BISHOP, position->squares[cap_sq]));
                move_list_push_back(&moves, move_promotion(from, cap_sq, KNIGHT, position->squares[cap_sq]));
            }
            else
            {
                move_list_push_back(&moves, move_capture(from, cap_sq, PAWN, position->squares[cap_sq]));
            }
        }
        square_t ep = position->state.en_passant_square;
        if (ep != 0 && ((pawn_attacks & bitboard_from_square(ep))))
        {
            move_list_push_back(&moves, move_ep_capture(from, ep));
        }
    }

    // piece_t moves.
    for (int p = 0; p < 4; ++p)
    {
        piece_t     piece     = PIECE_ATTACKERS_EXCEPT_KING[p].piece;
        attack_fn_t fn        = PIECE_ATTACKERS_EXCEPT_KING[p].fn;
        bitboard_t  pieces_bb = (position_pieces_of_type(position, piece) & friendly_pieces);
        BB_FOREACH(from, pieces_bb)
        {
            const bitboard_t attacks = fn(occupied_squares, from);
            bitboard_t       cap     = (attacks & enemy_pieces);
            bitboard_t       non_cap = (attacks & vacant_squares);
            square_t         to_sq;
            BB_FOREACH(to_sq, cap)
            move_list_push_back(&moves, move_capture(from, to_sq, piece, position->squares[to_sq]));
            BB_FOREACH(to_sq, non_cap)
            move_list_push_back(&moves, move_non_capture(from, to_sq, piece));
        }
    }
    // King moves.
    {
        bitboard_t king_bb = (position->kings & friendly_pieces);
        BB_FOREACH(from, king_bb)
        {
            const bitboard_t attacks = KING_ATTACKS[from];
            bitboard_t       cap     = (attacks & enemy_pieces);
            bitboard_t       non_cap = (attacks & vacant_squares);
            square_t         to_sq;
            BB_FOREACH(to_sq, cap)
            move_list_push_back(&moves, move_capture(from, to_sq, KING, position->squares[to_sq]));
            BB_FOREACH(to_sq, non_cap)
            move_list_push_back(&moves, move_non_capture(from, to_sq, KING));
        }
    }

    // Castling moves.
    if (!position_is_in_check(position))
    {
        static const bitboard_t F1_G1    = (1ull << 5) | (1ull << 6);
        static const bitboard_t B1_C1_D1 = (1ull << 1) | (1ull << 2) | (1ull << 3);
        static const bitboard_t F8_G8    = (1ull << 61) | (1ull << 62);
        static const bitboard_t B8_C8_D8 = (1ull << 57) | (1ull << 58) | (1ull << 59);
        static const square_t   SQ_E1    = 4;
        static const square_t   SQ_G1    = 6;
        static const square_t   SQ_C1    = 2;
        static const square_t   SQ_F1    = 5;
        static const square_t   SQ_D1    = 3;
        static const square_t   SQ_E8    = 60;
        static const square_t   SQ_G8    = 62;
        static const square_t   SQ_C8    = 58;
        static const square_t   SQ_F8    = 61;
        static const square_t   SQ_D8    = 59;
        if (color == WHITE)
        {
            if ((position->state.castling_rights & CASTLING_WHITE_KINGSIDE) && !((occupied_squares & F1_G1)) &&
                !position_is_attacked(position, SQ_F1, BLACK) && !position_is_attacked(position, SQ_G1, BLACK))
            {
                move_list_push_back(&moves, move_castling(SQ_E1, SQ_G1));
            }
            if ((position->state.castling_rights & CASTLING_WHITE_QUEENSIDE) && !((occupied_squares & B1_C1_D1)) &&
                !position_is_attacked(position, SQ_D1, BLACK) && !position_is_attacked(position, SQ_C1, BLACK))
            {
                move_list_push_back(&moves, move_castling(SQ_E1, SQ_C1));
            }
        }
        else
        {
            if ((position->state.castling_rights & CASTLING_BLACK_KINGSIDE) && !((occupied_squares & F8_G8)) &&
                !position_is_attacked(position, SQ_F8, WHITE) && !position_is_attacked(position, SQ_G8, WHITE))
            {
                move_list_push_back(&moves, move_castling(SQ_E8, SQ_G8));
            }
            if ((position->state.castling_rights & CASTLING_BLACK_QUEENSIDE) && !((occupied_squares & B8_C8_D8)) &&
                !position_is_attacked(position, SQ_D8, WHITE) && !position_is_attacked(position, SQ_C8, WHITE))
            {
                move_list_push_back(&moves, move_castling(SQ_E8, SQ_C8));
            }
        }
    }
    return moves;
}

// ---------------------------------------------------------------------------
// Comparison for sorting moves (by raw bits, ignoring score)
// ---------------------------------------------------------------------------

static int compare_move_bits(const void *a, const void *b)
{
    const move_t *ma = (const move_t *)a;
    const move_t *mb = (const move_t *)b;
    int64_t       va = *ma & 0x3FFFFF;
    int64_t       vb = *mb & 0x3FFFFF;
    if (va < vb)
    {
        return -1;
    }
    if (va > vb)
    {
        return 1;
    }
    return 0;
}

static void perft_regression(position_t *position, int depth, uint64_t *num_moves)
{
    move_list_t   legal_list  = position_generate_legal_moves(position);
    move_list_t   pseudo_list = generate_pseudo_legal_moves(position);
    const color_t color       = position_color_to_move(position);

    // Filter pseudo-legal moves: keep only truly legal ones.
    move_list_t validated;
    validated.size = 0;
    for (int i = 0; i < pseudo_list.size; ++i)
    {
        move_undo_t undo;
        position_make_move(position, pseudo_list.items[i], &undo);
        if (!position_is_attacked(position, position->king_location[color], enemy_of(color)))
        {
            move_list_push_back(&validated, pseudo_list.items[i]);
        }
        position_undo_move(position, pseudo_list.items[i], &undo);
    }

    // Sort both lists for set-like comparison (sort on lower 22 bits).
    move_list_t sorted_legal     = legal_list;
    move_list_t sorted_validated = validated;
    qsort(sorted_legal.items, (size_t)sorted_legal.size, sizeof(move_t), compare_move_bits);
    qsort(sorted_validated.items, (size_t)sorted_validated.size, sizeof(move_t), compare_move_bits);

    char pos_buf[128], mv_buf[8];
    position_to_string(position, pos_buf, sizeof(pos_buf));

    // Check legal ⊆ validated.
    for (int i = 0; i < sorted_legal.size; ++i)
    {
        move_t needle = sorted_legal.items[i];
        bool   found  = false;
        for (int j = 0; j < sorted_validated.size; ++j)
        {
            if (move_equals(sorted_validated.items[j], needle))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            move_to_string(needle, mv_buf, sizeof(mv_buf));
            printf("Error on position %s missing move %s\n", pos_buf, mv_buf);
        }
    }
    // Check validated ⊆ legal.
    for (int i = 0; i < sorted_validated.size; ++i)
    {
        move_t needle = sorted_validated.items[i];
        bool   found  = false;
        for (int j = 0; j < sorted_legal.size; ++j)
        {
            if (move_equals(sorted_legal.items[j], needle))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            move_to_string(needle, mv_buf, sizeof(mv_buf));
            printf("Error on position %s superfluous move %s\n", pos_buf, mv_buf);
        }
    }

    if (depth > 1)
    {
        for (int i = 0; i < legal_list.size; ++i)
        {
            move_undo_t undo;
            position_make_move(position, legal_list.items[i], &undo);
            perft_regression(position, depth - 1, num_moves);
            position_undo_move(position, legal_list.items[i], &undo);
        }
        return;
    }
    *num_moves += (uint64_t)sorted_legal.size;
}

// ---------------------------------------------------------------------------
// Main runner
// ---------------------------------------------------------------------------

void run_perft_tests(bool do_regression)
{
    // Parse perft_results into perft_test_t entries.
    // Upper bound: 132 entries each with up to 7 depth/count pairs.
    static perft_test_t tests[132 * 7];
    static char         fen_storage[132][128]; // persistent FEN copies, one per perft_results entry
    int                 num_tests = 0;

    for (int r = 0; r < 132 && perft_results[r]; ++r)
    {
        const char *line = perft_results[r];

        // Copy FEN (up to the first ';') into stable storage.
        const char *semi = strchr(line, ';');
        if (!semi)
        {
            continue;
        }
        int fen_len = (int)(semi - line);
        if (fen_len >= (int)sizeof(fen_storage[r]))
        {
            fen_len = (int)sizeof(fen_storage[r]) - 1;
        }
        memcpy(fen_storage[r], line, (size_t)fen_len);
        fen_storage[r][fen_len] = '\0';

        // Parse depth/count tokens from the rest of the string.
        char rest[512];
        strncpy(rest, semi + 1, sizeof(rest) - 1);
        rest[sizeof(rest) - 1] = '\0';

        char *tok = strtok(rest, ";");
        while (tok)
        {
            if (num_tests < (int)(sizeof(tests) / sizeof(tests[0])))
            {
                int      depth;
                uint64_t count;
                if (sscanf(tok, " D%d %" PRIu64, &depth, &count) == 2)
                {
                    tests[num_tests].position = fen_storage[r];
                    tests[num_tests].depth    = depth;
                    tests[num_tests].count    = count;
                    ++num_tests;
                }
            }
            tok = strtok(NULL, ";");
        }
    }

    bool            is_good     = true;
    uint64_t        total_nodes = 0;
    struct timespec first_start;
    clock_gettime(CLOCK_MONOTONIC, &first_start);

    for (int i = 0; i < num_tests; ++i)
    {
        position_t position = position_from_string(tests[i].position);
        char       pos_buf[128];
        position_to_string(&position, pos_buf, sizeof(pos_buf));

        uint64_t num_moves = 0;
        total_nodes += tests[i].count;

        struct timespec start, stop;
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (do_regression)
        {
            perft_regression(&position, tests[i].depth, &num_moves);
        }
        else
        {
            perft(&position, tests[i].depth, &num_moves);
        }
        clock_gettime(CLOCK_MONOTONIC, &stop);

        int64_t elapsed_us =
            (int64_t)(stop.tv_sec - start.tv_sec) * 1000000LL + (stop.tv_nsec - start.tv_nsec) / 1000LL;
        if (elapsed_us == 0)
        {
            elapsed_us = 1;
        }

        printf("%-75s depth:%2d moves:%10" PRIu64 " Mnps:%4" PRIu64 "\n", pos_buf, tests[i].depth, num_moves,
               num_moves / (uint64_t)elapsed_us);
        fflush(stdout);

        if (num_moves != tests[i].count)
        {
            printf("ERROR: expected %" PRIu64 " got %" PRIu64 "\n", tests[i].count, num_moves);
            is_good = false;
        }
    }

    struct timespec last_stop;
    clock_gettime(CLOCK_MONOTONIC, &last_stop);
    int64_t total_ms = (int64_t)(last_stop.tv_sec - first_start.tv_sec) * 1000LL +
                       (last_stop.tv_nsec - first_start.tv_nsec) / 1000000LL;
    if (total_ms == 0)
    {
        total_ms = 1;
    }

    printf("%-40s%10lld\n", "total elapsed milliseconds", (long long)total_ms);
    printf("%-40s%10lld\n", "mean positions per second", (long long)(total_nodes * 1000 / (uint64_t)total_ms));
    printf("%s\n", is_good ? "******************* PERFT PASS *******************"
                           : "******************* PERFT FAIL *******************");
    fflush(stdout);
}
