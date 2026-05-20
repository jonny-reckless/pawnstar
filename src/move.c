#include "move.h"
#include <stdlib.h>

void move_to_string(move_t m, char *buf, size_t buf_size)
{
    if (buf_size < 6)
        return; // "a1b8q\0" = 6 bytes minimum
    square_to_string(move_from(m), buf, buf_size);
    square_to_string(move_to(m), buf + 2, buf_size - 2);
    piece_t p = move_promoted(m);
    if (p != NONE)
    {
        buf[4] = " pnbrqk"[p];
        buf[5] = '\0';
    }
    else
    {
        buf[4] = '\0';
    }
}

static int move_compare_desc(const void *a, const void *b)
{
    int sa = move_score(*(const move_t *)a);
    int sb = move_score(*(const move_t *)b);
    return (sa < sb) - (sa > sb); // descending
}

void move_list_sort(move_list_t *moves)
{
    qsort(moves->items, (size_t)moves->size, sizeof(move_t), move_compare_desc);
}

void move_list_stable_sort(move_list_t *moves)
{
    // Insertion sort (stable, O(n^2) — root node has ≤ MAX_MOVES_PER_POSITION moves).
    for (int i = 1; i < moves->size; i++)
    {
        move_t key = moves->items[i];
        int    j   = i - 1;
        while (j >= 0 && move_score(moves->items[j]) < move_score(key))
        {
            moves->items[j + 1] = moves->items[j];
            j--;
        }
        moves->items[j + 1] = key;
    }
}
