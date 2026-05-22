/// @file Google Test suite for move-generation correctness (perft node counts).
///
/// Each test case is a (FEN, depth, expected_node_count) triple parsed from the
/// perft_results[] table in tests/perft_results.c. Depths above MAX_PERFT_DEPTH
/// are skipped.

#define DEBUGX 0

extern "C"
{
#include "move.h"
#include "position.h"
}

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

extern "C"
{
    extern const char *perft_results[132];
}

static constexpr int MAX_PERFT_DEPTH = 6;

// ─────────────────────────────────────────────────────────────────────────────

struct PerftCase
{
    std::string fen;
    int         depth;
    uint64_t    expected;
    int         pos_idx; // index into perft_results[], used for test naming
};

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

static std::vector<PerftCase> build_cases()
{
    std::vector<PerftCase> cases;
    for (int r = 0; r < 132 && perft_results[r]; ++r)
    {
        const char *line = perft_results[r];
        const char *semi = std::strchr(line, ';');
        if (!semi)
            continue;

        std::string fen(line, semi - line);

        char rest[512];
        std::strncpy(rest, semi + 1, sizeof(rest) - 1);
        rest[sizeof(rest) - 1] = '\0';

        char *tok = std::strtok(rest, ";");
        while (tok)
        {
            int      depth;
            uint64_t count;
            if (std::sscanf(tok, " D%d %" SCNu64, &depth, &count) == 2 && depth <= MAX_PERFT_DEPTH)
                cases.push_back({fen, depth, count, r});
            tok = std::strtok(nullptr, ";");
        }
    }
    return cases;
}

// ─── Parametrized test ───────────────────────────────────────────────────────

class PerftTest : public testing::TestWithParam<PerftCase>
{
};

TEST_P(PerftTest, NodeCount)
{
    const PerftCase &tc  = GetParam();
    position_t       pos = position_from_string(tc.fen.c_str());

    auto     t0    = std::chrono::steady_clock::now();
    uint64_t nodes = count_nodes(&pos, tc.depth);
    auto     t1    = std::chrono::steady_clock::now();

    double elapsed_s = std::chrono::duration<double>(t1 - t0).count();
    double nps       = elapsed_s > 0.0 ? nodes / elapsed_s : 0.0;

    std::printf("  %-72s  D%d  %12" PRIu64 "  %7.3fs  %10.0f nps\n",
                tc.fen.c_str(), tc.depth, nodes, elapsed_s, nps);
    std::fflush(stdout);

    EXPECT_EQ(nodes, tc.expected) << "FEN: " << tc.fen << "  depth: " << tc.depth;
}

INSTANTIATE_TEST_SUITE_P(
    Standard,
    PerftTest,
    testing::ValuesIn(build_cases()),
    [](const testing::TestParamInfo<PerftCase> &info)
    {
        return "pos" + std::to_string(info.param.pos_idx) + "_D" + std::to_string(info.param.depth);
    });
