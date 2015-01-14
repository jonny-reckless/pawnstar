#pragma once
/******************************************************************************
Integral type definitions
*******************************************************************************/
typedef __int64             int64;
typedef unsigned __int64    bitboard;
typedef unsigned char       uchar;
/******************************************************************************
Piece types
*******************************************************************************/
enum Piece
{
    NO_PIECE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    MAX_PIECE,
};
/******************************************************************************
Colors
*******************************************************************************/
enum Color
{
    WHITE,
    BLACK,
    NEITHER_COLOR,
};
/******************************************************************************
Position castling flags
*******************************************************************************/
enum CastleFlags
{
    MAY_WHITE_K         = 0x01, // white has the right to castle king side
    MAY_WHITE_Q         = 0x02, // white has the right to castle queen side
    MAY_BLACK_K         = 0x04, // black has the right to castle king side
    MAY_BLACK_Q         = 0x08, // black has the right to castle queen side
    ALL_CASTLING_RIGHTS = (MAY_WHITE_K | MAY_WHITE_Q | MAY_BLACK_K | MAY_BLACK_Q),
};
/******************************************************************************
Position state flags
*******************************************************************************/
enum StateFlags
{
    IS_BLACK_TO_MOVE    = 0x01, // it is black's turn to move
    IS_CHECK            = 0x02, // is the side to move in check
    MOVED_INTO_CHECK    = 0x04, // is the side not to move in check
    IS_CHECKMATE        = 0x08, // position represents checkmate
    IS_STALEMATE        = 0x10, // position represents stalemate
    IS_DRAW_REPETITION  = 0x20, // position represents draw by repetition
    IS_DRAW_MATERIAL    = 0x40, // position represents draw by insufficient material
    IS_DRAW_50_MOVES    = 0x80, // position represents draw by the 50 move rule
    IS_GAME_DRAWN       = (IS_STALEMATE | IS_DRAW_REPETITION | IS_DRAW_MATERIAL | IS_DRAW_50_MOVES),
    IS_GAME_OVER        = (IS_GAME_DRAWN | IS_CHECKMATE),
};
/******************************************************************************
Individual square indices (little endian rank file mapping)
*******************************************************************************/
enum Squares
{
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
};
/******************************************************************************
Move type
*******************************************************************************/
enum MoveType
{
    MOVE_REGULAR,
    MOVE_PROMOTION_KNIGHT = KNIGHT,
    MOVE_PROMOTION_BISHOP = BISHOP,
    MOVE_PROMOTION_ROOK   = ROOK,
    MOVE_PROMOTION_QUEEN  = QUEEN,    
    MOVE_EN_PASSANT_CAPTURE,
    MOVE_CASTLING,
};
