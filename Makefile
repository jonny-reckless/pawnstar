PROGRAM             = pawnstar
CC                  = clang
CFLAGS              = -I include -D DEBUGX=1 -D _POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -std=c17 -mbmi2
BUILD_DIR           = build
PROGRAM_EXE         = $(BUILD_DIR)/$(PROGRAM)
GENERATED_DATA      = src/generated_data.c
GENERATOR_SOURCE    = generate_constants/generate_constants.c
GENERATOR_EXE       = generate_constants/gen_constants
OBJECTS             = $(addprefix $(BUILD_DIR)/, $(SOURCES:.c=.o))
DEPS                = $(OBJECTS:.o=.d)
SOURCES             = \
	chess_clock.c \
	history_table.c \
	pins.c \
	debug_hashtable.c \
	evaluation.c \
	game.c \
	input_handling.c \
	main.c \
	move.c \
	opening_book.c \
	position.c \
	search_alphabeta.c \
	search_quiescent.c \
	search_root_node.c \
	static_exchange_evaluation.c \
	transposition_table.c \
	$(notdir $(GENERATED_DATA))

ifeq ($(DEBUG), 1)
	CFLAGS += -g -O0 -D DEBUG -fsanitize=undefined -fsanitize=address
else
	CFLAGS += -g -O3 -D NDEBUG
endif

TEST_BUILD_DIR  = build-test
TEST_CFLAGS     = -I include -D DEBUGX=1 -D _POSIX_C_SOURCE=200809L -std=c17 -mbmi2 -O3 -D NDEBUG -Wall -Wextra

# Engine sources for test builds: same as the main build minus main.c and input_handling.c.
TEST_ENGINE_SRCS = \
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

TEST_ENGINE_OBJS = $(addprefix $(TEST_BUILD_DIR)/, $(TEST_ENGINE_SRCS:.c=.o))
TEST_EXES        = $(TEST_BUILD_DIR)/test_perft $(TEST_BUILD_DIR)/test_bratko_kopec $(TEST_BUILD_DIR)/test_see

.PHONY: all prep clean gen test

all: prep $(PROGRAM_EXE)

prep:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(PROGRAM_EXE) $(OBJECTS) $(DEPS) $(GENERATED_DATA) $(GENERATOR_EXE)
	rm -rf $(TEST_BUILD_DIR)

test: $(TEST_EXES)
	$(TEST_BUILD_DIR)/test_perft
	$(TEST_BUILD_DIR)/test_bratko_kopec
	$(TEST_BUILD_DIR)/test_see

$(TEST_BUILD_DIR):
	mkdir -p $@

$(TEST_BUILD_DIR)/%.o: src/%.c | $(TEST_BUILD_DIR)
	$(CC) -c $(TEST_CFLAGS) -o $@ $<

$(TEST_BUILD_DIR)/test_perft.o: tests/test_perft.c | $(TEST_BUILD_DIR)
	$(CC) -c $(TEST_CFLAGS) -o $@ $<

$(TEST_BUILD_DIR)/perft_results.o: tests/perft_results.c | $(TEST_BUILD_DIR)
	$(CC) -c $(TEST_CFLAGS) -o $@ $<

$(TEST_BUILD_DIR)/test_see.o: tests/test_see.c | $(TEST_BUILD_DIR)
	$(CC) -c $(TEST_CFLAGS) -o $@ $<

$(TEST_BUILD_DIR)/test_bratko_kopec.o: tests/test_bratko_kopec.c | $(TEST_BUILD_DIR)
	$(CC) -c $(TEST_CFLAGS) -o $@ $<

$(TEST_BUILD_DIR)/test_perft: $(TEST_ENGINE_OBJS) $(TEST_BUILD_DIR)/test_perft.o $(TEST_BUILD_DIR)/perft_results.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

$(TEST_BUILD_DIR)/test_bratko_kopec: $(TEST_ENGINE_OBJS) $(TEST_BUILD_DIR)/test_bratko_kopec.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

$(TEST_BUILD_DIR)/test_see: $(TEST_ENGINE_OBJS) $(TEST_BUILD_DIR)/test_see.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

gen: $(GENERATOR_EXE)

# Compile an object from source, generating a .d dependency file alongside it.
$(BUILD_DIR)/%.o: src/%.c
	$(CC) -c $(CFLAGS) -MMD -MP -o $@ $<

# Link the executable.
$(PROGRAM_EXE): $(OBJECTS)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $(PROGRAM_EXE) $(OBJECTS)

# Compile the generator executable.
$(GENERATOR_EXE): $(GENERATOR_SOURCE)
	$(CC) $(CFLAGS) -o $(GENERATOR_EXE) $(GENERATOR_SOURCE)

# Invoke the generator executable to create the generated data source file.
$(GENERATED_DATA): $(GENERATOR_EXE)
	$(GENERATOR_EXE) > $(GENERATED_DATA)

-include $(DEPS)
