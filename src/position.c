#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "debug_hashtable.h"
#include "generated_data.h"
#include "pins.h"
#include "position.h"

static const square_t   SQ_A1       = 0;
static const square_t   SQ_C1       = 2;
static const square_t   SQ_D1       = 3;
static const square_t   SQ_E1       = 4;
static const square_t   SQ_F1       = 5;
static const square_t   SQ_G1       = 6;
static const square_t   SQ_H1       = 7;
static const square_t   SQ_A8       = 56;
static const square_t   SQ_C8       = 58;
static const square_t   SQ_D8       = 59;
static const square_t   SQ_E8       = 60;
static const square_t   SQ_F8       = 61;
static const square_t   SQ_G8       = 62;
static const square_t   SQ_H8       = 63;
static const bitboard_t BB_F1_G1    = (1ull << 5) | (1ull << 6);
static const bitboard_t BB_B1_C1_D1 = (1ull << 1) | (1ull << 2) | (1ull << 3);
static const bitboard_t BB_F8_G8    = (1ull << 61) | (1ull << 62);
static const bitboard_t BB_B8_C8_D8 = (1ull << 57) | (1ull << 58) | (1ull << 59);

/// @brief Low-level move generator. If @p do_all_moves is false, generates only captures.
static move_list_t position_gen_moves(const position_t *pos, color_t color, bool do_all_moves);

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// No-hash piece helpers (used during undo — hash is restored from undo record)
// ---------------------------------------------------------------------------

static void position_add_piece_nohash(position_t *pos, color_t color, piece_t piece, square_t to)
{
    const bitboard_t to_bb = bitboard_from_square(to);
    (&pos->pawns)[piece - PAWN] ^= to_bb;
    (&pos->white_pieces)[color] ^= to_bb;
    pos->squares[to] = piece;
}

static void position_remove_piece_nohash(position_t *pos, color_t color, piece_t piece, square_t from)
{
    const bitboard_t from_bb = bitboard_from_square(from);
    (&pos->pawns)[piece - PAWN] ^= from_bb;
    (&pos->white_pieces)[color] ^= from_bb;
    pos->squares[from] = NONE;
}

static void position_move_piece_nohash(position_t *pos, color_t color, piece_t piece, square_t from, square_t to)
{
    const bitboard_t from_to_bb = bitboard_from_square(from) | bitboard_from_square(to);
    (&pos->pawns)[piece - PAWN] ^= from_to_bb;
    (&pos->white_pieces)[color] ^= from_to_bb;
    pos->squares[from] = NONE;
    pos->squares[to]   = piece;
}

static void position_add_piece(position_t *pos, color_t color, piece_t piece, square_t to)
{
    const bitboard_t to_bb = bitboard_from_square(to);
    (&pos->pawns)[piece - PAWN] ^= to_bb;
    (&pos->white_pieces)[color] ^= to_bb;
    pos->state.hash ^= PIECE_SQUARE_HASHES[color][piece - 1][to];
    pos->squares[to] = (uint8_t)piece;
}

static void position_remove_piece(position_t *pos, color_t color, piece_t piece, square_t from)
{
    const bitboard_t from_bb = bitboard_from_square(from);
    (&pos->pawns)[piece - PAWN] ^= from_bb;
    (&pos->white_pieces)[color] ^= from_bb;
    pos->state.hash ^= PIECE_SQUARE_HASHES[color][piece - 1][from];
    pos->squares[from] = (uint8_t)NONE;
}

static void position_move_piece(position_t *pos, color_t color, piece_t piece, square_t from, square_t to)
{
    const bitboard_t from_to_bb = bitboard_from_square(from) | bitboard_from_square(to);
    const zobrist_t *hash_row   = PIECE_SQUARE_HASHES[color][piece - 1];
    (&pos->pawns)[piece - PAWN] ^= from_to_bb;
    (&pos->white_pieces)[color] ^= from_to_bb;
    pos->state.hash ^= hash_row[to] ^ hash_row[from];
    pos->squares[from] = (uint8_t)NONE;
    pos->squares[to]   = (uint8_t)piece;
}

static zobrist_t position_compute_hash(const position_t *pos)
{
    zobrist_t hash = (pos->state.flags & POSITION_FLAG_BLACK_TO_MOVE) ? BLACK_MOVE_HASH : 0ull;
    hash ^= castling_rights_hash(pos->state.castling_rights);
    hash ^= EN_PASSANT_HASHES[pos->state.en_passant_square];
    for (piece_t piece = PAWN; piece <= KING; ++piece)
    {
        square_t s;
        BB_FOREACH(s, position_pieces_of_type(pos, piece) & pos->white_pieces)
        {
            hash ^= PIECE_SQUARE_HASHES[WHITE][piece - 1][s];
        }
        BB_FOREACH(s, position_pieces_of_type(pos, piece) & pos->black_pieces)
        {
            hash ^= PIECE_SQUARE_HASHES[BLACK][piece - 1][s];
        }
    }
    return hash;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static void position_save_undo(move_undo_t *undo, const position_t *pos)
{
    *undo = pos->state;
}

static void position_restore_undo(position_t *pos, const move_undo_t *undo)
{
    pos->state = *undo;
}

void position_make_null_move(position_t *pos, move_undo_t *undo)
{
    position_save_undo(undo, pos);

    pos->state.hash ^= EN_PASSANT_HASHES[pos->state.en_passant_square];
    pos->state.en_passant_square = 0;
    pos->state.hash ^= BLACK_MOVE_HASH;
    pos->state.flags |= POSITION_FLAG_NULL_MOVE;
    pos->state.flags ^= POSITION_FLAG_BLACK_TO_MOVE;
}

void position_undo_null_move(position_t *pos, const move_undo_t *undo)
{
    position_restore_undo(pos, undo);
}

void position_make_move(position_t *pos, move_t move, move_undo_t *undo)
{
    const color_t  color = position_color_to_move(pos);
    const square_t from  = move_from(move);
    const square_t to    = move_to(move);
    const piece_t  piece = move_piece(move);

    position_save_undo(undo, pos);

    pos->state.flags &= ~POSITION_FLAG_NULL_MOVE;
    const castling_rights_t new_castling_rights = castling_rights_after_move(pos->state.castling_rights, from, to);
    pos->state.hash ^= castling_rights_hash(new_castling_rights) ^ castling_rights_hash(pos->state.castling_rights);
    pos->state.castling_rights = new_castling_rights;
    pos->state.hash ^= EN_PASSANT_HASHES[pos->state.en_passant_square];
    pos->state.en_passant_square = 0;

    switch (move_type(move))
    {
    case MOVE_NON_CAPTURE:
        ++pos->state.reversible_move_count;
        position_move_piece(pos, color, piece, from, to);
        break;

    case MOVE_CAPTURE:
        pos->state.reversible_move_count = 0;
        position_remove_piece(pos, enemy_of(color), move_captured(move), to);
        position_move_piece(pos, color, piece, from, to);
        break;

    case MOVE_PROMOTION_KNIGHT:
    case MOVE_PROMOTION_BISHOP:
    case MOVE_PROMOTION_ROOK:
    case MOVE_PROMOTION_QUEEN:
        pos->state.reversible_move_count = 0;
        if (move_captured(move) != NONE)
        {
            position_remove_piece(pos, enemy_of(color), move_captured(move), to);
        }
        position_remove_piece(pos, color, PAWN, from);
        position_add_piece(pos, color, move_promoted(move), to);
        break;

    case MOVE_PAWN_DOUBLE_PUSH:
        pos->state.reversible_move_count = 0;
        position_move_piece(pos, color, PAWN, from, to);
        pos->state.en_passant_square = (square_t)((from + to) >> 1);
        pos->state.hash ^= EN_PASSANT_HASHES[pos->state.en_passant_square];
        break;

    case MOVE_EP_CAPTURE:
        pos->state.reversible_move_count = 0;
        position_remove_piece(pos, enemy_of(color), PAWN, (square_t)((from & 0x38) | (to & 0x07)));
        position_move_piece(pos, color, PAWN, from, to);
        break;

    case MOVE_CASTLING:
        pos->state.reversible_move_count = 0;
        switch (to)
        {
        case 6: // G1
            position_move_piece(pos, WHITE, KING, SQ_E1, SQ_G1);
            position_move_piece(pos, WHITE, ROOK, SQ_H1, SQ_F1);
            break;
        case 2: // C1
            position_move_piece(pos, WHITE, KING, SQ_E1, SQ_C1);
            position_move_piece(pos, WHITE, ROOK, SQ_A1, SQ_D1);
            break;
        case 62: // G8
            position_move_piece(pos, BLACK, KING, SQ_E8, SQ_G8);
            position_move_piece(pos, BLACK, ROOK, SQ_H8, SQ_F8);
            break;
        case 58: // C8
            position_move_piece(pos, BLACK, KING, SQ_E8, SQ_C8);
            position_move_piece(pos, BLACK, ROOK, SQ_A8, SQ_D8);
            break;
        default:
            break;
        }
        break;
    }

    pos->state.flags ^= POSITION_FLAG_BLACK_TO_MOVE;
    pos->state.hash ^= BLACK_MOVE_HASH;
    pos->state.full_move_count += (uint8_t)color;
    pos->king_location[color] = lsb(pos->kings & position_pieces_of_color(pos, color));
    pos->state.checkers       = position_attacks_to(pos, pos->king_location[enemy_of(color)], color);
}

void position_undo_move(position_t *pos, move_t move, const move_undo_t *undo)
{
    const color_t  color = enemy_of(position_color_to_move(pos));
    const square_t from  = move_from(move);
    const square_t to    = move_to(move);
    const piece_t  piece = move_piece(move);

    switch (move_type(move))
    {
    case MOVE_NON_CAPTURE:
    case MOVE_PAWN_DOUBLE_PUSH:
        position_move_piece_nohash(pos, color, piece, to, from);
        break;

    case MOVE_CAPTURE:
        position_move_piece_nohash(pos, color, piece, to, from);
        position_add_piece_nohash(pos, enemy_of(color), move_captured(move), to);
        break;

    case MOVE_PROMOTION_KNIGHT:
    case MOVE_PROMOTION_BISHOP:
    case MOVE_PROMOTION_ROOK:
    case MOVE_PROMOTION_QUEEN:
        position_remove_piece_nohash(pos, color, move_promoted(move), to);
        position_add_piece_nohash(pos, color, PAWN, from);
        if (move_captured(move) != NONE)
        {
            position_add_piece_nohash(pos, enemy_of(color), move_captured(move), to);
        }
        break;

    case MOVE_EP_CAPTURE:
        position_move_piece_nohash(pos, color, PAWN, to, from);
        position_add_piece_nohash(pos, enemy_of(color), PAWN, (square_t)((from & 0x38) | (to & 0x07)));
        break;

    case MOVE_CASTLING:
        switch (to)
        {
        case 6: // G1
            position_move_piece_nohash(pos, WHITE, KING, SQ_G1, SQ_E1);
            position_move_piece_nohash(pos, WHITE, ROOK, SQ_F1, SQ_H1);
            break;
        case 2: // C1
            position_move_piece_nohash(pos, WHITE, KING, SQ_C1, SQ_E1);
            position_move_piece_nohash(pos, WHITE, ROOK, SQ_D1, SQ_A1);
            break;
        case 62: // G8
            position_move_piece_nohash(pos, BLACK, KING, SQ_G8, SQ_E8);
            position_move_piece_nohash(pos, BLACK, ROOK, SQ_F8, SQ_H8);
            break;
        case 58: // C8
            position_move_piece_nohash(pos, BLACK, KING, SQ_C8, SQ_E8);
            position_move_piece_nohash(pos, BLACK, ROOK, SQ_D8, SQ_A8);
            break;
        default:
            break;
        }
        break;
    }

    pos->king_location[color] = lsb(pos->kings & position_pieces_of_color(pos, color));
    position_restore_undo(pos, undo);
}

position_t position_from_string(const char *fen_string)
{
    position_t pos;
    memset(&pos, 0, sizeof(pos));
    const char *p = fen_string;

    // piece_t placement
    int x = 0, y = 7;
    while (*p && *p != ' ')
    {
        char c = *p++;
        if (c == '/')
        {
            x = 0;
            --y;
            continue;
        }
        if (c >= '1' && c <= '8')
        {
            x += c - '0';
            continue;
        }
        const char *white_pieces = "PNBRQK";
        const char *black_pieces = "pnbrqk";
        const char *wp           = strchr(white_pieces, c);
        if (wp)
        {
            position_add_piece(&pos, WHITE, (piece_t)(int)(wp - white_pieces + 1), square_from_coords(x, y));
            x++;
            continue;
        }
        const char *bp = strchr(black_pieces, c);
        if (bp)
        {
            position_add_piece(&pos, BLACK, (piece_t)(int)(bp - black_pieces + 1), square_from_coords(x, y));
            x++;
            continue;
        }
    }
    if (*p == ' ')
    {
        p++;
    }

    // Side to move
    if (*p == 'b')
    {
        pos.state.flags |= POSITION_FLAG_BLACK_TO_MOVE;
    }
    while (*p && *p != ' ')
    {
        p++;
    }
    if (*p == ' ')
    {
        p++;
    }

    // Castling rights
    {
        char cr_buf[8] = {0};
        int  i         = 0;
        while (*p && *p != ' ' && i < 7)
        {
            cr_buf[i++] = *p++;
        }
        pos.state.castling_rights = castling_rights_from_fen(cr_buf);
    }
    while (*p && *p != ' ')
    {
        p++;
    }
    if (*p == ' ')
    {
        p++;
    }

    // En passant
    if (*p == '-')
    {
        pos.state.en_passant_square = (square_t)0;
        p++;
    }
    else
    {
        char ep_buf[4] = {0};
        int  i         = 0;
        while (*p && *p != ' ' && i < 3)
        {
            ep_buf[i++] = *p++;
        }
        pos.state.en_passant_square = square_from_string(ep_buf);
    }
    while (*p && *p != ' ')
    {
        p++;
    }
    if (*p == ' ')
    {
        p++;
    }

    // Optional half-move clock
    if (*p)
    {
        pos.state.reversible_move_count = (uint8_t)atoi(p);
        while (*p && *p != ' ')
        {
            p++;
        }
        if (*p == ' ')
        {
            p++;
        }
    }

    // Optional full-move number
    if (*p)
    {
        int fmn                   = atoi(p);
        pos.state.full_move_count = (uint8_t)(fmn > 0 ? fmn - 1 : 0);
    }

    pos.king_location[WHITE] = lsb(pos.kings & pos.white_pieces);
    pos.king_location[BLACK] = lsb(pos.kings & pos.black_pieces);
    pos.state.hash           = position_compute_hash(&pos);
    const color_t color      = position_color_to_move(&pos);
    pos.state.checkers       = position_attacks_to(&pos, pos.king_location[color], enemy_of(color));
    return pos;
}

void position_to_string(const position_t *pos, char *buf, size_t buf_size)
{
    if (buf_size == 0)
    {
        return;
    }
    char *p   = buf;
    char *end = buf + buf_size - 1;

    const bitboard_t occ = pos->white_pieces | pos->black_pieces;
    for (int rank = 7; rank >= 0; --rank)
    {
        int num_empty = 0;
        for (int file = 0; file < 8; ++file)
        {
            square_t   sq    = square_from_coords(file, rank);
            bitboard_t sq_bb = bitboard_from_square(sq);
            if (!(occ & sq_bb))
            {
                ++num_empty;
            }
            else
            {
                if (num_empty && p < end)
                {
                    *p++      = (char)('0' + num_empty);
                    num_empty = 0;
                }
                piece_t     piece    = pos->squares[sq];
                bool        is_white = pos->white_pieces & sq_bb;
                const char *chars    = is_white ? " PNBRQK" : " pnbrqk";
                if (p < end)
                {
                    *p++ = chars[piece];
                }
            }
        }
        if (num_empty && p < end)
        {
            *p++ = (char)('0' + num_empty);
        }
        if (rank > 0 && p < end)
        {
            *p++ = '/';
        }
    }

    if (p < end)
    {
        *p++ = ' ';
    }
    if (p < end)
    {
        *p++ = (pos->state.flags & POSITION_FLAG_BLACK_TO_MOVE) ? 'b' : 'w';
    }
    if (p < end)
    {
        *p++ = ' ';
    }

    char cr_buf[8];
    castling_rights_to_fen_string(pos->state.castling_rights, cr_buf, sizeof(cr_buf));
    for (const char *s = cr_buf; *s && p < end; s++)
    {
        *p++ = *s;
    }
    if (p < end)
    {
        *p++ = ' ';
    }

    if (pos->state.en_passant_square == 0)
    {
        if (p < end)
        {
            *p++ = '-';
        }
    }
    else
    {
        char ep_buf[4];
        square_to_string(pos->state.en_passant_square, ep_buf, sizeof(ep_buf));
        for (const char *s = ep_buf; *s && p < end; s++)
        {
            *p++ = *s;
        }
    }

    int written =
        snprintf(p, (size_t)(end - p + 1), " %d %d", pos->state.reversible_move_count, pos->state.full_move_count + 1);
    if (written > 0)
    {
        p += written;
    }

    *p = '\0';
}

bool position_is_draw_by_material(const position_t *pos)
{
    const bitboard_t occupied = pos->white_pieces | pos->black_pieces;
    switch (popcount(occupied))
    {
    case 0:
    case 1:
        printf("ERROR: too few pieces for play\n");
        return true;
    case 2:
        INCREMENT("draws by material (2)");
        return true;
    case 3:
        if (pos->bishops | pos->knights)
        {
            INCREMENT("draws by material (3)");
            return true;
        }
        return false;
    case 4: {
        const bitboard_t white_bishops = pos->bishops & pos->white_pieces;
        const bitboard_t black_bishops = pos->bishops & pos->black_pieces;
        const bool       wb_on_white   = white_bishops & WHITE_SQUARES;
        const bool       bb_on_white   = black_bishops & WHITE_SQUARES;
        if (white_bishops && black_bishops && wb_on_white == bb_on_white)
        {
            INCREMENT("draws by material (4)");
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

bool position_is_checkmate(const position_t *pos)
{
    return position_is_in_check(pos) && position_generate_legal_moves(pos).size == 0;
}

bool position_is_stalemate(const position_t *pos)
{
    return !position_is_in_check(pos) && position_generate_legal_moves(pos).size == 0;
}

move_list_t position_generate_legal_moves(const position_t *pos)
{
    color_t c = position_color_to_move(pos);
    return position_gen_moves(pos, c, true);
}

move_list_t position_generate_legal_captures(const position_t *pos)
{
    color_t c = position_color_to_move(pos);
    return position_gen_moves(pos, c, false);
}

// No-pins specialisation: identical to position_gen_moves but all pins_is_allowed /
// pins_allowed_squares calls are removed. Safe to call only when pins.has_pins is false.
static move_list_t position_gen_moves_no_pins(const position_t *pos, color_t color, bool do_all_moves,
                                              bitboard_t allowed_captures, bitboard_t allowed_non_captures)
{
    const bitboard_t occupied_squares = pos->white_pieces | pos->black_pieces;
    const bitboard_t friendly_pieces  = position_pieces_of_color(pos, color);
    const bitboard_t enemy_pieces     = occupied_squares ^ friendly_pieces;
    const bitboard_t enemy_pawns      = pos->pawns & enemy_pieces;
    const square_t   king_locn        = pos->king_location[color];

    move_list_t moves;
    moves.size = 0;

    // Squares the king cannot move to (attacked or x-ray attacked by enemy).
    bitboard_t forbidden_king_squares =
        color == WHITE ? (bitboard_shift_southwest(enemy_pawns) | bitboard_shift_southeast(enemy_pawns))
                       : (bitboard_shift_northwest(enemy_pawns) | bitboard_shift_northeast(enemy_pawns));

    // Remove our king so sliding attackers see through it (x-ray).
    const bitboard_t occupied_except_king = occupied_squares ^ bitboard_from_square(king_locn);
    for (int i = 0; i < 5; i++)
    {
        const piece_t     ep  = PIECE_ATTACKERS[i].piece;
        const attack_fn_t efn = PIECE_ATTACKERS[i].fn;
        const bitboard_t  eb  = position_pieces_of_type(pos, ep) & enemy_pieces;
        square_t          s;
        BB_FOREACH(s, eb)
        {
            forbidden_king_squares |= efn(occupied_except_king, s);
        }
    }

    // King moves.
    const bitboard_t king_move_targets = KING_ATTACKS[king_locn] & ~forbidden_king_squares;
    const bitboard_t king_captures     = king_move_targets & enemy_pieces;
    square_t         to;
    BB_FOREACH(to, king_captures)
    {
        move_list_push_back(&moves, move_capture(king_locn, to, KING, pos->squares[to]));
    }
    if (do_all_moves)
    {
        const bitboard_t king_non_captures = king_move_targets & ~occupied_squares;
        BB_FOREACH(to, king_non_captures)
        {
            move_list_push_back(&moves, move_non_capture(king_locn, to, KING));
        }
    }

    // Non-king piece moves (knight, bishop, rook, queen) — no pin masking needed.
    square_t from;
    for (int i = 0; i < 4; i++)
    {
        const piece_t     fp  = PIECE_ATTACKERS_EXCEPT_KING[i].piece;
        const attack_fn_t ffn = PIECE_ATTACKERS_EXCEPT_KING[i].fn;
        const bitboard_t  fb  = position_pieces_of_type(pos, fp) & friendly_pieces;
        BB_FOREACH(from, fb)
        {
            const bitboard_t attacks     = ffn(occupied_squares, from);
            const bitboard_t cap_targets = attacks & allowed_captures;
            BB_FOREACH(to, cap_targets)
            {
                move_list_push_back(&moves, move_capture(from, to, fp, pos->squares[to]));
            }
            if (do_all_moves)
            {
                const bitboard_t non_cap_targets = attacks & allowed_non_captures;
                BB_FOREACH(to, non_cap_targets)
                {
                    move_list_push_back(&moves, move_non_capture(from, to, fp));
                }
            }
        }
    }

    // Castling.
    if (do_all_moves && !position_is_in_check(pos))
    {
        if (color == WHITE)
        {
            if ((pos->state.castling_rights & CASTLING_WHITE_KINGSIDE) && !(occupied_squares & BB_F1_G1) &&
                !position_is_attacked(pos, SQ_F1, BLACK) && !position_is_attacked(pos, SQ_G1, BLACK))
            {
                move_list_push_back(&moves, move_castling(SQ_E1, SQ_G1));
            }
            if ((pos->state.castling_rights & CASTLING_WHITE_QUEENSIDE) && !(occupied_squares & BB_B1_C1_D1) &&
                !position_is_attacked(pos, SQ_D1, BLACK) && !position_is_attacked(pos, SQ_C1, BLACK))
            {
                move_list_push_back(&moves, move_castling(SQ_E1, SQ_C1));
            }
        }
        else
        {
            if ((pos->state.castling_rights & CASTLING_BLACK_KINGSIDE) && !(occupied_squares & BB_F8_G8) &&
                !position_is_attacked(pos, SQ_F8, WHITE) && !position_is_attacked(pos, SQ_G8, WHITE))
            {
                move_list_push_back(&moves, move_castling(SQ_E8, SQ_G8));
            }
            if ((pos->state.castling_rights & CASTLING_BLACK_QUEENSIDE) && !(occupied_squares & BB_B8_C8_D8) &&
                !position_is_attacked(pos, SQ_D8, WHITE) && !position_is_attacked(pos, SQ_C8, WHITE))
            {
                move_list_push_back(&moves, move_castling(SQ_E8, SQ_C8));
            }
        }
    }

    // Pawn moves.
    bitboard_t pawns, single_pushes, double_pushes, captures_west, captures_east, en_passant_sources;
    bitboard_t promotions, promotions_west, promotions_east;
    int        push_delta, west_delta, east_delta;

    if (color == WHITE)
    {
        pawns         = pos->pawns & pos->white_pieces;
        single_pushes = bitboard_shift_north(pawns) & ~occupied_squares;
        double_pushes = bitboard_shift_north(single_pushes) & ~occupied_squares & RANK4;
        captures_west = bitboard_shift_northwest(pawns) & pos->black_pieces;
        captures_east = bitboard_shift_northeast(pawns) & pos->black_pieces;
        en_passant_sources =
            pos->state.en_passant_square != 0 ? PAWN_ATTACKS_BLACK[pos->state.en_passant_square] & pawns : NO_SQUARES;
        promotions      = single_pushes & RANK8;
        promotions_west = captures_west & RANK8;
        promotions_east = captures_east & RANK8;
        push_delta      = 8;
        west_delta      = 7;
        east_delta      = 9;
    }
    else
    {
        pawns         = pos->pawns & pos->black_pieces;
        single_pushes = bitboard_shift_south(pawns) & ~occupied_squares;
        double_pushes = bitboard_shift_south(single_pushes) & ~occupied_squares & RANK5;
        captures_west = bitboard_shift_southwest(pawns) & pos->white_pieces;
        captures_east = bitboard_shift_southeast(pawns) & pos->white_pieces;
        en_passant_sources =
            pos->state.en_passant_square != 0 ? PAWN_ATTACKS_WHITE[pos->state.en_passant_square] & pawns : NO_SQUARES;
        promotions      = single_pushes & RANK1;
        promotions_west = captures_west & RANK1;
        promotions_east = captures_east & RANK1;
        push_delta      = -8;
        west_delta      = -9;
        east_delta      = -7;
    }
    captures_west ^= promotions_west;
    captures_east ^= promotions_east;
    single_pushes ^= promotions;

    typedef struct
    {
        bitboard_t bb;
        int        delta;
    } bb_delta_t;

    // Capture promotions — no pin check needed.
    const bb_delta_t promotion_captures[2] = {{promotions_west, west_delta}, {promotions_east, east_delta}};
    for (int i = 0; i < 2; i++)
    {
        const bitboard_t b = promotion_captures[i].bb & allowed_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - promotion_captures[i].delta);
            move_list_push_back(&moves, move_promotion(from, to, QUEEN, pos->squares[to]));
            move_list_push_back(&moves, move_promotion(from, to, ROOK, pos->squares[to]));
            move_list_push_back(&moves, move_promotion(from, to, BISHOP, pos->squares[to]));
            move_list_push_back(&moves, move_promotion(from, to, KNIGHT, pos->squares[to]));
        }
    }

    // Quiet promotions — no pin check needed.
    {
        const bitboard_t b = promotions & allowed_non_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - push_delta);
            move_list_push_back(&moves, move_promotion_quiet(from, to, QUEEN));
            move_list_push_back(&moves, move_promotion_quiet(from, to, ROOK));
            move_list_push_back(&moves, move_promotion_quiet(from, to, BISHOP));
            move_list_push_back(&moves, move_promotion_quiet(from, to, KNIGHT));
        }
    }

    // Non-promotion captures — no pin check needed.
    const bb_delta_t pawn_captures[2] = {{captures_west, west_delta}, {captures_east, east_delta}};
    for (int i = 0; i < 2; i++)
    {
        const bitboard_t b = pawn_captures[i].bb & allowed_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - pawn_captures[i].delta);
            move_list_push_back(&moves, move_capture(from, to, PAWN, pos->squares[to]));
        }
    }

    if (do_all_moves)
    {
        // Single pawn pushes — no pin check needed.
        bitboard_t b = single_pushes & allowed_non_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - push_delta);
            move_list_push_back(&moves, move_non_capture(from, to, PAWN));
        }
        // Double pawn pushes — no pin check needed.
        b = double_pushes & allowed_non_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - push_delta * 2);
            move_list_push_back(&moves, move_double_push(from, to));
        }
    }

    // En passant captures — no pin check, but the horizontal discovered-check test must stay.
    BB_FOREACH(from, en_passant_sources)
    {
        const square_t ep_to         = pos->state.en_passant_square;
        const square_t captured_pawn = (square_t)((from & 0x38) | (ep_to & 0x07));
        if (allowed_captures & bitboard_from_square(captured_pawn))
        {
            if (square_rank(king_locn) == square_rank(from))
            {
                // Test for the rare discovered horizontal check when removing both pawns from king's rank.
                const bitboard_t pseudo_occ =
                    occupied_squares ^ bitboard_from_square(captured_pawn) ^ bitboard_from_square(from);
                const bitboard_t horizontal = rook_attacks(pseudo_occ, king_locn) & (WEST[king_locn] | EAST[king_locn]);
                if (!(horizontal & (enemy_pieces & (pos->rooks | pos->queens))))
                {
                    move_list_push_back(&moves, move_ep_capture(from, ep_to));
                }
            }
            else
            {
                move_list_push_back(&moves, move_ep_capture(from, ep_to));
            }
        }
    }
    return moves;
}

move_list_t position_gen_moves(const position_t *pos, color_t color, bool do_all_moves)
{
    // Check restrictions: in check we can only capture the checker or interpose.
    bitboard_t allowed_captures     = position_pieces_of_color(pos, enemy_of(color));
    bitboard_t allowed_non_captures = ~(pos->white_pieces | pos->black_pieces);

    if (position_is_in_check(pos))
    {
        if (popcount(pos->state.checkers) > 1)
        {
            // Double check: only king moves are legal; generate them inline and return.
            const bitboard_t occupied_squares = pos->white_pieces | pos->black_pieces;
            const bitboard_t friendly_pieces  = position_pieces_of_color(pos, color);
            const bitboard_t enemy_pieces     = occupied_squares ^ friendly_pieces;
            const bitboard_t enemy_pawns      = pos->pawns & enemy_pieces;
            const square_t   king_locn        = pos->king_location[color];

            bitboard_t forbidden_king_squares =
                color == WHITE ? (bitboard_shift_southwest(enemy_pawns) | bitboard_shift_southeast(enemy_pawns))
                               : (bitboard_shift_northwest(enemy_pawns) | bitboard_shift_northeast(enemy_pawns));
            const bitboard_t occupied_except_king = occupied_squares ^ bitboard_from_square(king_locn);
            for (int i = 0; i < 5; i++)
            {
                const piece_t     piece           = PIECE_ATTACKERS[i].piece;
                const attack_fn_t attack_fn       = PIECE_ATTACKERS[i].fn;
                const bitboard_t  enemy_attackers = position_pieces_of_type(pos, piece) & enemy_pieces;
                square_t          s;
                BB_FOREACH(s, enemy_attackers)
                {
                    forbidden_king_squares |= attack_fn(occupied_except_king, s);
                }
            }
            const bitboard_t king_move_targets = KING_ATTACKS[king_locn] & ~forbidden_king_squares;
            move_list_t      moves;
            moves.size = 0;
            square_t to;
            BB_FOREACH(to, king_move_targets & enemy_pieces)
            {
                move_list_push_back(&moves, move_capture(king_locn, to, KING, pos->squares[to]));
            }
            if (do_all_moves)
            {
                BB_FOREACH(to, king_move_targets & ~occupied_squares)
                {
                    move_list_push_back(&moves, move_non_capture(king_locn, to, KING));
                }
            }
            return moves;
        }
        const square_t checker_locn = lsb(pos->state.checkers);
        allowed_captures            = pos->state.checkers;
        allowed_non_captures        = INTERVENING_SQUARES[pos->king_location[color]][checker_locn];
    }

    // Determine whether any piece is pinned and dispatch to the appropriate generator.
    pins_t pins;
    pins_compute(&pins, pos);
    if (!pins.pinned_pieces)
    {
        return position_gen_moves_no_pins(pos, color, do_all_moves, allowed_captures, allowed_non_captures);
    }

    const bitboard_t occupied_squares = pos->white_pieces | pos->black_pieces;
    const bitboard_t friendly_pieces  = position_pieces_of_color(pos, color);
    const bitboard_t enemy_pieces     = occupied_squares ^ friendly_pieces;
    const bitboard_t enemy_pawns      = pos->pawns & enemy_pieces;
    const square_t   king_locn        = pos->king_location[color];

    move_list_t moves;
    moves.size = 0;

    // Squares the king cannot move to (attacked or x-ray attacked by enemy).
    bitboard_t forbidden_king_squares =
        color == WHITE ? (bitboard_shift_southwest(enemy_pawns) | bitboard_shift_southeast(enemy_pawns))
                       : (bitboard_shift_northwest(enemy_pawns) | bitboard_shift_northeast(enemy_pawns));

    // Remove our king so sliding attackers see through it (x-ray).
    const bitboard_t occupied_except_king = occupied_squares ^ bitboard_from_square(king_locn);
    for (int i = 0; i < 5; i++)
    {
        const piece_t     ep  = PIECE_ATTACKERS[i].piece;
        const attack_fn_t efn = PIECE_ATTACKERS[i].fn;
        const bitboard_t  eb  = position_pieces_of_type(pos, ep) & enemy_pieces;
        square_t          s;
        BB_FOREACH(s, eb)
        {
            forbidden_king_squares |= efn(occupied_except_king, s);
        }
    }

    // King moves.
    const bitboard_t king_move_targets = KING_ATTACKS[king_locn] & ~forbidden_king_squares;
    const bitboard_t king_captures     = king_move_targets & enemy_pieces;
    square_t         to;
    BB_FOREACH(to, king_captures)
    {
        move_list_push_back(&moves, move_capture(king_locn, to, KING, pos->squares[to]));
    }
    if (do_all_moves)
    {
        const bitboard_t king_non_captures = king_move_targets & ~occupied_squares;
        BB_FOREACH(to, king_non_captures)
        {
            move_list_push_back(&moves, move_non_capture(king_locn, to, KING));
        }
    }

    // Non-king piece moves (knight, bishop, rook, queen).
    square_t from;
    for (int i = 0; i < 4; i++)
    {
        const piece_t     fp  = PIECE_ATTACKERS_EXCEPT_KING[i].piece;
        const attack_fn_t ffn = PIECE_ATTACKERS_EXCEPT_KING[i].fn;
        const bitboard_t  fb  = position_pieces_of_type(pos, fp) & friendly_pieces;
        BB_FOREACH(from, fb)
        {
            const bitboard_t attacks     = ffn(occupied_squares, from) & pins_allowed_squares(&pins, from);
            const bitboard_t cap_targets = attacks & allowed_captures;
            BB_FOREACH(to, cap_targets)
            {
                move_list_push_back(&moves, move_capture(from, to, fp, pos->squares[to]));
            }
            if (do_all_moves)
            {
                const bitboard_t non_cap_targets = attacks & allowed_non_captures;
                BB_FOREACH(to, non_cap_targets)
                {
                    move_list_push_back(&moves, move_non_capture(from, to, fp));
                }
            }
        }
    }

    // Castling.
    if (do_all_moves && !position_is_in_check(pos))
    {
        if (color == WHITE)
        {
            if ((pos->state.castling_rights & CASTLING_WHITE_KINGSIDE) && !(occupied_squares & BB_F1_G1) &&
                !position_is_attacked(pos, SQ_F1, BLACK) && !position_is_attacked(pos, SQ_G1, BLACK))
            {
                move_list_push_back(&moves, move_castling(SQ_E1, SQ_G1));
            }
            if ((pos->state.castling_rights & CASTLING_WHITE_QUEENSIDE) && !(occupied_squares & BB_B1_C1_D1) &&
                !position_is_attacked(pos, SQ_D1, BLACK) && !position_is_attacked(pos, SQ_C1, BLACK))
            {
                move_list_push_back(&moves, move_castling(SQ_E1, SQ_C1));
            }
        }
        else
        {
            if ((pos->state.castling_rights & CASTLING_BLACK_KINGSIDE) && !(occupied_squares & BB_F8_G8) &&
                !position_is_attacked(pos, SQ_F8, WHITE) && !position_is_attacked(pos, SQ_G8, WHITE))
            {
                move_list_push_back(&moves, move_castling(SQ_E8, SQ_G8));
            }
            if ((pos->state.castling_rights & CASTLING_BLACK_QUEENSIDE) && !(occupied_squares & BB_B8_C8_D8) &&
                !position_is_attacked(pos, SQ_D8, WHITE) && !position_is_attacked(pos, SQ_C8, WHITE))
            {
                move_list_push_back(&moves, move_castling(SQ_E8, SQ_C8));
            }
        }
    }

    // Pawn moves.
    bitboard_t pawns, single_pushes, double_pushes, captures_west, captures_east, en_passant_sources;
    bitboard_t promotions, promotions_west, promotions_east;
    int        push_delta, west_delta, east_delta;

    if (color == WHITE)
    {
        pawns         = pos->pawns & pos->white_pieces;
        single_pushes = bitboard_shift_north(pawns) & ~occupied_squares;
        double_pushes = bitboard_shift_north(single_pushes) & ~occupied_squares & RANK4;
        captures_west = bitboard_shift_northwest(pawns) & pos->black_pieces;
        captures_east = bitboard_shift_northeast(pawns) & pos->black_pieces;
        en_passant_sources =
            pos->state.en_passant_square != 0 ? PAWN_ATTACKS_BLACK[pos->state.en_passant_square] & pawns : NO_SQUARES;
        promotions      = single_pushes & RANK8;
        promotions_west = captures_west & RANK8;
        promotions_east = captures_east & RANK8;
        push_delta      = 8;
        west_delta      = 7;
        east_delta      = 9;
    }
    else
    {
        pawns         = pos->pawns & pos->black_pieces;
        single_pushes = bitboard_shift_south(pawns) & ~occupied_squares;
        double_pushes = bitboard_shift_south(single_pushes) & ~occupied_squares & RANK5;
        captures_west = bitboard_shift_southwest(pawns) & pos->white_pieces;
        captures_east = bitboard_shift_southeast(pawns) & pos->white_pieces;
        en_passant_sources =
            pos->state.en_passant_square != 0 ? PAWN_ATTACKS_WHITE[pos->state.en_passant_square] & pawns : NO_SQUARES;
        promotions      = single_pushes & RANK1;
        promotions_west = captures_west & RANK1;
        promotions_east = captures_east & RANK1;
        push_delta      = -8;
        west_delta      = -9;
        east_delta      = -7;
    }
    captures_west ^= promotions_west;
    captures_east ^= promotions_east;
    single_pushes ^= promotions;

    // Capture promotions.
    typedef struct
    {
        bitboard_t bb;
        int        delta;
    } bb_delta_t;
    const bb_delta_t promotion_captures[2] = {{promotions_west, west_delta}, {promotions_east, east_delta}};
    for (int i = 0; i < 2; i++)
    {
        const bitboard_t b = promotion_captures[i].bb & allowed_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - promotion_captures[i].delta);
            if (pins_is_allowed(&pins, from, to))
            {
                move_list_push_back(&moves, move_promotion(from, to, QUEEN, pos->squares[to]));
                move_list_push_back(&moves, move_promotion(from, to, ROOK, pos->squares[to]));
                move_list_push_back(&moves, move_promotion(from, to, BISHOP, pos->squares[to]));
                move_list_push_back(&moves, move_promotion(from, to, KNIGHT, pos->squares[to]));
            }
        }
    }

    // Quiet promotions.
    {
        const bitboard_t b = promotions & allowed_non_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - push_delta);
            if (pins_is_allowed(&pins, from, to))
            {
                move_list_push_back(&moves, move_promotion_quiet(from, to, QUEEN));
                move_list_push_back(&moves, move_promotion_quiet(from, to, ROOK));
                move_list_push_back(&moves, move_promotion_quiet(from, to, BISHOP));
                move_list_push_back(&moves, move_promotion_quiet(from, to, KNIGHT));
            }
        }
    }

    // Non-promotion captures.
    const bb_delta_t pawn_captures[2] = {{captures_west, west_delta}, {captures_east, east_delta}};
    for (int i = 0; i < 2; i++)
    {
        const bitboard_t b = pawn_captures[i].bb & allowed_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - pawn_captures[i].delta);
            if (pins_is_allowed(&pins, from, to))
            {
                move_list_push_back(&moves, move_capture(from, to, PAWN, pos->squares[to]));
            }
        }
    }

    if (do_all_moves)
    {
        // Single pawn pushes.
        bitboard_t b = single_pushes & allowed_non_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - push_delta);
            if (pins_is_allowed(&pins, from, to))
            {
                move_list_push_back(&moves, move_non_capture(from, to, PAWN));
            }
        }
        // Double pawn pushes.
        b = double_pushes & allowed_non_captures;
        BB_FOREACH(to, b)
        {
            from = (square_t)(to - push_delta * 2);
            if (pins_is_allowed(&pins, from, to))
            {
                move_list_push_back(&moves, move_double_push(from, to));
            }
        }
    }

    // En passant captures.
    // square_t from;
    BB_FOREACH(from, en_passant_sources)
    {
        const square_t ep_to         = pos->state.en_passant_square;
        const square_t captured_pawn = (square_t)((from & 0x38) | (ep_to & 0x07));
        if (pins_is_allowed(&pins, from, ep_to) && (allowed_captures & bitboard_from_square(captured_pawn)))
        {
            if (square_rank(king_locn) == square_rank(from))
            {
                // Test for the rare discovered horizontal check when removing both pawns from king's rank.
                const bitboard_t pseudo_occ =
                    occupied_squares ^ bitboard_from_square(captured_pawn) ^ bitboard_from_square(from);
                const bitboard_t horizontal = rook_attacks(pseudo_occ, king_locn) & (WEST[king_locn] | EAST[king_locn]);
                if (!(horizontal & (enemy_pieces & (pos->rooks | pos->queens))))
                {
                    move_list_push_back(&moves, move_ep_capture(from, ep_to));
                }
            }
            else
            {
                move_list_push_back(&moves, move_ep_capture(from, ep_to));
            }
        }
    }
    return moves;
}
