#include "pins.h"
#include "attacks.h"
#include "generated_data.h"
#include "position.h"

void pins_compute(pins_t *self, const position_t *pos)
{
    self->pinned_pieces            = NO_SQUARES;
    const color_t    color         = position_color_to_move(pos);
    const bitboard_t occupied      = position_occupied_squares(pos);
    const bitboard_t friendly      = position_pieces_of_color(pos, color);
    const square_t   king_loc      = position_king_location(pos, color);
    const bitboard_t enemy_sliding = (((BISHOP_ATTACKS[king_loc] & (position_bishops(pos) | position_queens(pos))) |
                                       (ROOK_ATTACKS[king_loc] & (position_rooks(pos) | position_queens(pos)))) &
                                      (~friendly));

    square_t s;
    BB_FOREACH(s, enemy_sliding)
    {
        const bitboard_t between        = INTERVENING_SQUARES[king_loc][s];
        const bitboard_t between_pieces = (between & occupied);
        const bitboard_t pinned         = (between_pieces & friendly);
        if ((pinned) && bitboard_pop_count(between_pieces) == 1)
        {
            self->pinned_pieces |= pinned;
            self->allowed_squares[bitboard_lsb(pinned)] = (between | bitboard_from_square(s));
        }
    }
}
