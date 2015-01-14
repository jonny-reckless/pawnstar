#pragma once
#pragma warning(disable:4201)

#include <string>

#include "types.h"
#include "move.h"
#include "macros.h"

struct MoveUndoCtx
{    
    uchar   castle_flags;    
    uchar   state_flags;     
    uchar   en_passant_square;
    uchar   half_move_clock; 
    uchar   move_count;
    uchar   captured_piece;
    uchar   king_location[2];
};

struct Position
{    
    union
    {
        bitboard pieces[7];        
        struct
        {
            bitboard occupied_squares;
            bitboard pawns;
            bitboard knights;
            bitboard bishops;
            bitboard rooks;
            bitboard queens;
            bitboard kings;
        };
    };
    union
    {
        bitboard pieces_of_color[2]; 
        struct
        {
            bitboard white_pieces;
            bitboard black_pieces;
        };
    };
    uchar           piece_on[64];   
    MoveUndoCtx     ctx;

    Position();
    Position(const std::string& fen_string);
    Position(const Position& that);
    Position& operator=(const Position& that);

    bool MakeMove(Move move, MoveUndoCtx& undo_ctx);
    void UndoMove(Move move, const MoveUndoCtx& undo_ctx);  
    bool IsAttacked(int location, int color) const;
    bitboard AttacksFromSquare(int location) const;
    bitboard AttacksToSquare(int location) const;
    bitboard AttacksToSquare(int location, int color) const;
    bool IsLegal() const;
    inline int ColorToMove() const;
    inline int ColorAt(int location) const;
    inline bool IsInCheck() const;
    std::string ToString() const;
};

inline int Position::ColorToMove() const
{
    return ctx.state_flags & IS_BLACK_TO_MOVE ? BLACK : WHITE;
}

inline int Position::ColorAt(int location) const
{
    return BITBOARD(location) & white_pieces ? WHITE : BITBOARD(location) & black_pieces ? BLACK : NEITHER_COLOR;
}

inline bool Position::IsInCheck() const
{
    return !!(ctx.state_flags & IS_CHECK);
}