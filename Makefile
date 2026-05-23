PROGRAM          = pawnstar
CC               = clang
CFLAGS           = -I include -D DEBUGX=1 -D _POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -std=c17 -mbmi2
BUILD_DIR        = build
PROGRAM_EXE      = $(BUILD_DIR)/$(PROGRAM)
GENERATED_DATA   = src/generated_data.c
GENERATOR_SOURCE = generate_constants/generate_constants.c
GENERATOR_EXE    = generate_constants/gen_constants

ifeq ($(DEBUG), 1)
	CFLAGS += -g -O0 -D DEBUG -fsanitize=undefined -fsanitize=address
else
	CFLAGS += -g -O3 -D NDEBUG
endif

# ─── Engine static library ────────────────────────────────────────────────────
# Excludes main.c and input_handling.c (entry point and UCI command loop).
LIB_SOURCES = \
	chess_clock.c \
	debug_hashtable.c \
	evaluation.c \
	game.c \
	history_table.c \
	move.c \
	opening_book.c \
	pins.c \
	position.c \
	search_alphabeta.c \
	search_quiescent.c \
	search_root_node.c \
	static_exchange_evaluation.c \
	transposition_table.c \
	$(notdir $(GENERATED_DATA))

LIB_OBJECTS  = $(addprefix $(BUILD_DIR)/, $(LIB_SOURCES:.c=.o))
LIB          = $(BUILD_DIR)/libpawnstar.a

MAIN_SOURCES = input_handling.c main.c
MAIN_OBJECTS = $(addprefix $(BUILD_DIR)/, $(MAIN_SOURCES:.c=.o))

DEPS = $(LIB_OBJECTS:.o=.d) $(MAIN_OBJECTS:.o=.d)

# ─── Test executables ────────────────────────────────────────────────────────
TEST_BUILD_DIR = build-test
TEST_EXES      = $(TEST_BUILD_DIR)/test_perft $(TEST_BUILD_DIR)/test_bratko_kopec $(TEST_BUILD_DIR)/test_see

# ─── Top-level targets ───────────────────────────────────────────────────────
.PHONY: all prep clean gen test

all: prep $(PROGRAM_EXE)

prep:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(PROGRAM_EXE) $(LIB) $(LIB_OBJECTS) $(MAIN_OBJECTS) $(DEPS) $(GENERATED_DATA) $(GENERATOR_EXE)
	rm -rf $(TEST_BUILD_DIR) $(BUILD_DIR)

# ─── Main build ──────────────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: src/%.c | prep
	$(CC) -c $(CFLAGS) -MMD -MP -o $@ $<

$(LIB): $(LIB_OBJECTS)
	ar rcs $@ $^

$(PROGRAM_EXE): $(MAIN_OBJECTS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(MAIN_OBJECTS) $(LIB)

# ─── Code generator ──────────────────────────────────────────────────────────
gen: $(GENERATOR_EXE)

$(GENERATOR_EXE): $(GENERATOR_SOURCE)
	$(CC) $(CFLAGS) -o $(GENERATOR_EXE) $(GENERATOR_SOURCE)

$(GENERATED_DATA): $(GENERATOR_EXE)
	$(GENERATOR_EXE) > $(GENERATED_DATA)

# ─── Test build ──────────────────────────────────────────────────────────────
test: prep $(LIB) $(TEST_EXES)

check: test
	$(TEST_BUILD_DIR)/test_perft 5
	$(TEST_BUILD_DIR)/test_bratko_kopec
	$(TEST_BUILD_DIR)/test_see

$(TEST_BUILD_DIR):
	mkdir -p $@

$(TEST_BUILD_DIR)/%.o: tests/%.c | $(TEST_BUILD_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(TEST_BUILD_DIR)/test_perft: $(TEST_BUILD_DIR)/test_perft.o $(TEST_BUILD_DIR)/perft_results.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_BUILD_DIR)/test_perft.o $(TEST_BUILD_DIR)/perft_results.o $(LIB)

$(TEST_BUILD_DIR)/test_bratko_kopec: $(TEST_BUILD_DIR)/test_bratko_kopec.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_BUILD_DIR)/test_bratko_kopec.o $(LIB)

$(TEST_BUILD_DIR)/test_see: $(TEST_BUILD_DIR)/test_see.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_BUILD_DIR)/test_see.o $(LIB)

-include $(DEPS)
