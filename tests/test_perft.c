/// @file Standalone perft test: move-generation node counts (D1–D6).

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "move.h"
#include "position.h"

extern const char *perft_results[132];

#define PERFT_STACK_SIZE 8

static uint64_t count_nodes(position_t *stack, int sp, int depth)
{
    position_t *pos = &stack[sp];
    move_list_t ml  = position_generate_legal_moves(pos);
    if (depth <= 1)
        return (uint64_t)ml.size;
    uint64_t n = 0;
    for (int i = 0; i < ml.size; ++i)
    {
        position_make_move(&stack[sp + 1], pos, ml.items[i]);
        n += count_nodes(stack, sp + 1, depth - 1);
    }
    return n;
}

int main(int argc, char *argv[])
{
    int max_depth = 5;
    if (argc >= 2)
        max_depth = atoi(argv[1]);

    int     failures      = 0;
    int     total         = 0;
    int64_t total_nodes   = 0;
    double  total_elapsed = 0.0;

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
            if (sscanf(tok, " D%d %" SCNu64, &depth, &expected) == 2 && depth <= max_depth)
            {
                position_t stack[PERFT_STACK_SIZE];
                stack[0] = position_from_string(fen);

                struct timespec t0, t1;
                clock_gettime(CLOCK_MONOTONIC, &t0);
                uint64_t got = count_nodes(stack, 0, depth);
                clock_gettime(CLOCK_MONOTONIC, &t1);

                double elapsed_s = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
                double mnps      = elapsed_s > 0.0 ? (double)got / elapsed_s / 1e6 : 0.0;

                total_nodes += got;
                total_elapsed += elapsed_s;

                printf("  %-72s  D%d  %12" PRIu64 "  %7.3fs  %8.3f Mnps", fen, depth, got, elapsed_s, mnps);
                if (got != expected)
                {
                    printf("[FAIL] (expected %" PRIu64 ")", expected);
                    failures++;
                }
                printf("\n");
                fflush(stdout);
                total++;
            }
            tok = strtok(NULL, ";");
        }
    }

    double mean_mnps = total_elapsed > 0.0 ? total_nodes / total_elapsed / 1e6 : 0.0;
    printf("\n%d/%d tests passed. Mean perft speed %.3f Mnps\n", total - failures, total, mean_mnps);
    return failures > 0 ? 1 : 0;
}
