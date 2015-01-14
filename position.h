#pragma once
#pragma warning(disable:4201)

#include <string>

#include "types.h"
#include "move.h"

struct PositionFlags
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
    PositionFlags   flags;

    Position();
    Position(const std::string& fen_string);
    Position(const Position& that);
    Position& operator=(const Position& that);

    bool MakeMove(Move move, PositionFlags& flags);
    void UndoMove(Move move, const PositionFlags& flags);  
    bool IsAttacked(int location, int color) const;
    bitboard AttacksFromSquare(int location) const;
    bitboard AttacksToSquare(int location) const;

    int ColorToMove() const
    {
        return flags.state_flags & IS_BLACK_TO_MOVE ? BLACK : WHITE;
    }

    bool IsInCheck() const
    {
        return !!(flags.state_flags & IS_CHECK);
    }
};