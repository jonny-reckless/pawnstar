#include "pawnstar.h"

void RunStaticExchangeTests(void)
{
    const char* const fenString = "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -";
    Position position[1];
    const int move = CONSTRUCT_MOVE(19, 36, KNIGHT, CAPTURE, PAWN);
    if (!PositionFromString(fenString, position))
    {
        printf("ERROR: unable to produce position from string\n");
        return;
    }
    printf("%s; static exchange score for Nxe5 = %d\n", fenString, EvaluateStaticExchange(position, move));
    printf("done\n");
}