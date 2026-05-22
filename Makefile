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
	perft_results.c \
	position.c \
	search_alphabeta.c \
	search_quiescent.c \
	search_root_node.c \
	static_exchange_evaluation.c \
	tests_bratko_kopec.c \
	tests_move_generation.c \
	tests_static_exchange.c \
	transposition_table.c \
	$(notdir $(GENERATED_DATA))

ifeq ($(DEBUG), 1)
	CFLAGS += -g -O0 -D DEBUG -fsanitize=undefined -fsanitize=address
else
	CFLAGS += -g -O3 -D NDEBUG
endif

.PHONY: all prep clean gen

all: prep $(PROGRAM_EXE)

prep:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(PROGRAM_EXE) $(OBJECTS) $(DEPS) $(GENERATED_DATA) $(GENERATOR_EXE)

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
