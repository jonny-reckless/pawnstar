/// @file Standalone perft test: move-generation node counts (D1–D6).

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "move.h"
#include "position.h"

extern const char *perft_results[132];

#define MAX_PERFT_DEPTH 6

static uint64_t count_nodes(position_t *pos, int depth)
{
    move_list_t ml = position_generate_legal_moves(pos);
    if (depth <= 1)
        return (uint64_t)ml.size;
    uint64_t n = 0;
    for (int i = 0; i < ml.size; ++i)
    {
        move_undo_t undo;
        position_make_move(pos, ml.items[i], &undo);
        n += count_nodes(pos, depth - 1);
        position_undo_move(pos, ml.items[i], &undo);
    }
    return n;
}

int main(void)
{
    int failures = 0;
    int total    = 0;

    for (int r = 0; r < 132 && perft_results[r]; ++r)
    {
        const char *line = perft_results[r];
        const char *semi = strchr(line, ';');
        if (!semi)
            continue;

        char fen[256];
        int  fen_len = (int)(semi - line);
        if (fen_len >= (int)sizeof(fen))
            fen_len = (int)sizeof(fen) - 1;
        memcpy(fen, line, (size_t)fen_len);
        fen[fen_len] = '\0';

        char rest[512];
        strncpy(rest, semi + 1, sizeof(rest) - 1);
        rest[sizeof(rest) - 1] = '\0';

        char *tok = strtok(rest, ";");
        while (tok)
        {
            int      depth;
            uint64_t expected;
            if (sscanf(tok, " D%d %" SCNu64, &depth, &expected) == 2 && depth <= MAX_PERFT_DEPTH)
            {
                position_t pos = position_from_string(fen);

                struct timespec t0, t1;
                clock_gettime(CLOCK_MONOTONIC, &t0);
                uint64_t got = count_nodes(&pos, depth);
                clock_gettime(CLOCK_MONOTONIC, &t1);

                double elapsed_s =
                    (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
                double nps = elapsed_s > 0.0 ? (double)got / elapsed_s : 0.0;

                printf("  %-72s  D%d  %12" PRIu64 "  %7.3fs  %10.0f nps", fen, depth, got,
                       elapsed_s, nps);
                if (got != expected)
                {
                    printf("  FAIL (expected %" PRIu64 ")", expected);
                    failures++;
                }
                printf("\n");
                fflush(stdout);
                total++;
            }
            tok = strtok(NULL, ";");
        }
    }

    printf("\n%d/%d passed\n", total - failures, total);
    return failures > 0 ? 1 : 0;
}
