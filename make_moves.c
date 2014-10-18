#include "pawnstar.h"

#define IS_IN_CHECK(position, color) (IsAttacked(position, position->kingLocation[color], ENEMY(color)))
/******************************************************************************
Castling rights masks are ANDed with the castleFlags for the move source and
destination squares to determine new castling rights following a move

Only moves involving squares a1, e1, h1, a8, e8 or h8 affect castling rights
*******************************************************************************/
static const uchar CASTLING_RIGHTS_MASKS[64] =
{
    0xFD, 0xFF, 0xFF, 0xFF, 0xFC, 0xFF, 0xFF, 0xFE, // 1
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 2
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 3
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 4
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 5
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 6
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 7
    0xF7, 0xFF, 0xFF, 0xFF, 0xF3, 0xFF, 0xFF, 0xFB, // 8
};//  a     b     c     d     e     f     g     h
/******************************************************************************
Make a "null" move (used for null move pruning during search)
*******************************************************************************/
void MakeNullMove(Position* dstPosition, const Position* srcPosition)
{
    *dstPosition = *srcPosition;
    dstPosition->previous = srcPosition;
    dstPosition->move = 0;
    if (dstPosition->enPassantIndex)
    {
        dstPosition->hash ^= EN_PASSANT_HASHES[FILE_OF(dstPosition->enPassantIndex)];
        dstPosition->enPassantIndex = 0;
    }
    dstPosition->stateFlags ^= IS_BLACK_TO_MOVE;
    dstPosition->hash = ~dstPosition->hash;
}
/******************************************************************************
Construct a new position from an old position and a move
*******************************************************************************/
void MakeMove(Position* dstPosition, const Position* srcPosition, int move)
{
    int enPassantCaptureLocation;
    bitboard enPassantCaptureBB;    
    int promoted;                  
    const int color                       = COLOR_TO_MOVE(srcPosition);
    const int enemy                        = ENEMY(color);
    const int from                         = MOVE_FROM(move);
    const int to                           = MOVE_TO(move);
    const int piece                        = MOVE_PIECE(move);
    const int type                         = MOVE_TYPE(move);
    const int captured                     = MOVE_CAPTURED(move);
    const bitboard fromBB                   = BITBOARD(from);
    const bitboard toBB                     = BITBOARD(to);
    const bitboard fromToBB                 = fromBB | toBB;
    *dstPosition                            = *srcPosition;
    dstPosition->previous                   = srcPosition;
    dstPosition->move                       = move;
    dstPosition->castleFlags &= CASTLING_RIGHTS_MASKS[from] & CASTLING_RIGHTS_MASKS[to];
    if (dstPosition->castleFlags != srcPosition->castleFlags)
    {
        dstPosition->hash ^=
            CASTLING_RIGHTS_HASHES[dstPosition->castleFlags & ALL_RIGHTS] ^
            CASTLING_RIGHTS_HASHES[srcPosition->castleFlags & ALL_RIGHTS];
    }
    if (dstPosition->enPassantIndex)
    {
        dstPosition->hash ^= EN_PASSANT_HASHES[FILE_OF(dstPosition->enPassantIndex)];
        dstPosition->enPassantIndex = 0;
    }
    if (piece == KING)
    {
        dstPosition->kingLocation[color] = (uchar)to;
    }
    switch (type)
    {
    case CAPTURE:
        dstPosition->reversibleMoveCount = 0;
        dstPosition->pieces[captured]   ^= toBB;
        dstPosition->allPieces[enemy]   ^= toBB;
        dstPosition->pieces[piece]      ^= fromToBB;
        dstPosition->allPieces[color]  ^= fromToBB;
        dstPosition->occupiedSquares    ^= fromBB;
        dstPosition->hash ^=
            PIECE_SQUARE_HASHES[color][piece][to]   ^
            PIECE_SQUARE_HASHES[color][piece][from] ^
            PIECE_SQUARE_HASHES[enemy][captured][to];       
        break;
    case NON_CAPTURE:
        ++dstPosition->reversibleMoveCount;
        dstPosition->pieces[piece]     ^= fromToBB;
        dstPosition->allPieces[color] ^= fromToBB;
        dstPosition->occupiedSquares   ^= fromToBB;
        dstPosition->hash ^=
            PIECE_SQUARE_HASHES[color][piece][to] ^
            PIECE_SQUARE_HASHES[color][piece][from];
        break;
    case DOUBLE_PAWN_PUSH:
        dstPosition->enPassantIndex = (uchar)((from + to) >> 1);
        dstPosition->reversibleMoveCount = 0;
        dstPosition->pawns             ^= fromToBB;
        dstPosition->allPieces[color] ^= fromToBB;
        dstPosition->occupiedSquares   ^= fromToBB;
        dstPosition->hash ^=
            PIECE_SQUARE_HASHES[color][PAWN][to]   ^
            PIECE_SQUARE_HASHES[color][PAWN][from] ^
            EN_PASSANT_HASHES[FILE_OF(from)];
        break;
    case SINGLE_PAWN_PUSH:
        dstPosition->reversibleMoveCount = 0;
        dstPosition->pawns              ^= fromToBB;
        dstPosition->allPieces[color]  ^= fromToBB;
        dstPosition->occupiedSquares    ^= fromToBB;
        dstPosition->hash ^=
            PIECE_SQUARE_HASHES[color][PAWN][to] ^
            PIECE_SQUARE_HASHES[color][PAWN][from];
        break;
    case EN_PASSANT_CAPTURE:
        dstPosition->reversibleMoveCount = 0;
        enPassantCaptureLocation = (from & 0x38) | (to & 0x07); // captured location is source rank, destination file
        enPassantCaptureBB = BITBOARD(enPassantCaptureLocation);       
        dstPosition->allPieces[color] ^= fromToBB;
        dstPosition->allPieces[enemy]  ^= enPassantCaptureBB;
        dstPosition->pawns             ^= fromToBB | enPassantCaptureBB;
        dstPosition->occupiedSquares   ^= fromToBB | enPassantCaptureBB;
        dstPosition->hash ^=
            PIECE_SQUARE_HASHES[color][PAWN][to]   ^
            PIECE_SQUARE_HASHES[color][PAWN][from] ^
            PIECE_SQUARE_HASHES[enemy][PAWN][enPassantCaptureLocation];
        break;
    case PAWN_PROMOTION_CAPTURE:
        dstPosition->reversibleMoveCount = 0;
        promoted = MOVE_PROMOTED(move);
        dstPosition->pieces[captured]  ^= toBB;
        dstPosition->allPieces[enemy]  ^= toBB;
        dstPosition->pawns             ^= fromBB;
        dstPosition->pieces[promoted]  ^= toBB;
        dstPosition->allPieces[color] ^= fromToBB;
        dstPosition->occupiedSquares   ^= fromBB;
        dstPosition->hash ^=
            PIECE_SQUARE_HASHES[color][PAWN][from]   ^
            PIECE_SQUARE_HASHES[color][promoted][to] ^
            PIECE_SQUARE_HASHES[enemy][captured][to];
        break;
    case PAWN_PROMOTION_NON_CAPTURE:
        dstPosition->reversibleMoveCount = 0;
        promoted = MOVE_PROMOTED(move);
        dstPosition->pawns             ^= fromBB;
        dstPosition->pieces[promoted]  ^= toBB;
        dstPosition->allPieces[color] ^= fromToBB;
        dstPosition->occupiedSquares   ^= fromToBB;
        dstPosition->hash ^=
            PIECE_SQUARE_HASHES[color][PAWN][from] ^
            PIECE_SQUARE_HASHES[color][promoted][to];
        break;
    case CASTLING:
        dstPosition->reversibleMoveCount = 0;
        switch (to)
        {
        case G1:
            /******************************************************************
            white castles king side
            *******************************************************************/
            dstPosition->kings ^= E1BB | G1BB;
            dstPosition->rooks ^= H1BB | F1BB;
            dstPosition->whitePieces     ^= E1BB | F1BB | G1BB | H1BB;
            dstPosition->occupiedSquares ^= E1BB | F1BB | G1BB | H1BB;
            dstPosition->hash ^=
                PIECE_SQUARE_HASHES[WHITE][KING][E1] ^
                PIECE_SQUARE_HASHES[WHITE][KING][G1] ^
                PIECE_SQUARE_HASHES[WHITE][ROOK][H1] ^
                PIECE_SQUARE_HASHES[WHITE][ROOK][F1];
            dstPosition->castleFlags |= HAS_WHITE_CASTLED;
            break;
        case C1:
            /******************************************************************
            white castles queen side
            *******************************************************************/
            dstPosition->kings ^= E1BB | C1BB;
            dstPosition->rooks ^= A1BB | D1BB;
            dstPosition->whitePieces     ^= A1BB | C1BB | D1BB | E1BB;
            dstPosition->occupiedSquares ^= A1BB | C1BB | D1BB | E1BB;
            dstPosition->hash ^=
                PIECE_SQUARE_HASHES[WHITE][KING][E1] ^
                PIECE_SQUARE_HASHES[WHITE][KING][C1] ^
                PIECE_SQUARE_HASHES[WHITE][ROOK][A1] ^
                PIECE_SQUARE_HASHES[WHITE][ROOK][D1];
            dstPosition->castleFlags |= HAS_WHITE_CASTLED;
            break;
        case G8:
            /******************************************************************
            black castles king side
            *******************************************************************/
            dstPosition->kings ^= E8BB | G8BB;
            dstPosition->rooks ^= H8BB | F8BB;
            dstPosition->blackPieces     ^= E8BB | F8BB | G8BB | H8BB;
            dstPosition->occupiedSquares ^= E8BB | F8BB | G8BB | H8BB;
            dstPosition->hash ^=
                PIECE_SQUARE_HASHES[BLACK][KING][E8] ^
                PIECE_SQUARE_HASHES[BLACK][KING][G8] ^
                PIECE_SQUARE_HASHES[BLACK][ROOK][H8] ^
                PIECE_SQUARE_HASHES[BLACK][ROOK][F8];
            dstPosition->castleFlags |= HAS_BLACK_CASTLED;
            break;
        case C8:
            /******************************************************************
            black castles queen side
            *******************************************************************/
            dstPosition->kings ^= E8BB | C8BB;
            dstPosition->rooks ^= A8BB | D8BB;
            dstPosition->blackPieces     ^= A8BB | C8BB | D8BB | E8BB;
            dstPosition->occupiedSquares ^= A8BB | C8BB | D8BB | E8BB;
            dstPosition->hash ^=
                PIECE_SQUARE_HASHES[BLACK][KING][E8] ^
                PIECE_SQUARE_HASHES[BLACK][KING][C8] ^
                PIECE_SQUARE_HASHES[BLACK][ROOK][A8] ^
                PIECE_SQUARE_HASHES[BLACK][ROOK][D8];
            dstPosition->castleFlags |= HAS_BLACK_CASTLED;
            break;
        default:
            break;
        }
    }
    dstPosition->stateFlags ^= IS_BLACK_TO_MOVE;
    dstPosition->hash = ~dstPosition->hash;
    dstPosition->fullMoveCount += (color == BLACK);
    if (IS_IN_CHECK(dstPosition, color))
    {
        dstPosition->stateFlags |= WAS_MOVE_ILLEGAL;
    }
    else
    {
        if (IS_IN_CHECK(dstPosition, ENEMY(color)))
        {
            dstPosition->stateFlags |= IS_CHECK;
        }
        else
        {
            dstPosition->stateFlags &= ~IS_CHECK;
        }
    }
}
/******************************************************************************
Make a move and update the game end status flags
*******************************************************************************/
void PlayMove(Game* game, int move)
{
    if (game->position->stateFlags & IS_GAME_OVER)
    {
        printf("ERROR: attempt to play move after game is over\n");
        return;
    }
    MakeMove(game->position + 1, game->position, move);
    ++game->position;
    if (IsCheckmate(game->position))
    {
        game->position->stateFlags |= IS_CHECKMATE;
    }
    else if (IsStalemate(game->position))
    {
        game->position->stateFlags |= IS_STALEMATE;
    }
    else if (IsDrawByRepetition(game->position, false))
    {
        game->position->stateFlags |= IS_DRAW_REPETITION;
    }
    else if (IsDrawByMaterial(game->position))
    {
        game->position->stateFlags |= IS_DRAW_MATERIAL;
    }
    else if (IsDrawByFiftyMoves(game->position))
    {
        game->position->stateFlags |= IS_DRAW_50_MOVES;
    }
}
/******************************************************************************
Play a move from the given move string in either standard algebraic or XBoard 
format, and update game stateFlags accordingly

returns the move if a legal move was played
returns zero if the move was illegal or the game is over
*******************************************************************************/
int PlayMoveString(Game* game, char* moveStr, bool isSAN)
{
    int moves[MAX_MOVES_PER_POSITION];
    const int* move;
    if (game->position->stateFlags & IS_GAME_OVER)
    {
        return 0;
    }    
    GenerateLegalMoves(game->position, moves);
    for (move = moves; *move; ++move)
    {
        char buffer[16];
        if (isSAN)
        {
            MoveToSanString(game->position, *move,  buffer);
        }
        else
        {
            MoveToString(*move, buffer);
        }
        if (AreMoveStringsEqual(buffer, moveStr))
        {
            PlayMove(game, *move);
            return *move;
        }
    }
    return 0;
}
