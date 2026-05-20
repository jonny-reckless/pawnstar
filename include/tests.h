#pragma once
/// @file Test-suite entry points, invoked from the UCI command loop.
#include <stdbool.h>

/// @brief Run PERFT move-generation tests against known node counts.
/// If @p do_regression is true, also validates legal-move lists against an independent pseudo-legal generator.
void run_perft_tests(bool do_regression);

/// @brief Run Bratko-Kopec tactical positions searching to the given @p depth.
void run_position_tests(int depth);

/// @brief Run static exchange evaluation tests against expected scores.
void run_static_exchange_tests(void);
