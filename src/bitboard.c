#include "bitboard.h"

void bitboard_to_string(bitboard_t b, char *buf, size_t buf_size)
{
    if (buf_size < 73)
    {
        return; // 8 rows of 8 chars + 8 newlines + null
    }
    int pos = 0;
    for (int y = 7; y >= 0; --y)
    {
        for (int x = 0; x < 8; ++x)
        {
            buf[pos++] = (b & (1ull << (x + 8 * y))) ? '1' : '0';
        }
        buf[pos++] = '\n';
    }
    buf[pos] = '\0';
}
