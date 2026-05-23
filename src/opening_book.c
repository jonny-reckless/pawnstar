/// @file Opening book: maps Zobrist hashes to available book moves.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "move.h"
#include "opening_book.h"
#include "position.h"
#include "square.h"

static const char *START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/// @brief XOR shift PRNG
/// @param s state
/// @return next random number
static inline uint64_t xorshift64(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return (*s = x);
}

/// @brief Find a book entry from its Zobrist key.
/// @param self Opening book.
/// @param key Zobrist key.
/// @return Found entry or NULL if not found.
static book_entry_t *find_entry(const opening_book_t *self, zobrist_t key)
{
    for (int i = 0; i < self->count; ++i)
    {
        if (self->entries[i].key == key)
        {
            return &self->entries[i];
        }
    }
    return NULL;
}

/// @brief Create a new book entry and add it to the book. NB not yet sorted,
/// @param self Opening book.
/// @param key Zobrist hash.
/// @return Newkly created entry.
static book_entry_t *create_entry(opening_book_t *self, zobrist_t key)
{
    if (self->count == self->capacity)
    {
        self->capacity = self->capacity ? self->capacity * 2 : 32;
        self->entries  = realloc(self->entries, self->capacity * sizeof(book_entry_t));
    }
    book_entry_t *entry = &self->entries[self->count++];
    entry->key          = key;
    entry->moves        = NULL;
    entry->count        = 0;
    entry->capacity     = 0;
    return entry;
}

/// @brief Add a move to an exisiting book entry (position).
/// @param e Book entry.
/// @param m Move to append.
static void entry_push_move(book_entry_t *e, move_t m)
{
    if (e->count == e->capacity)
    {
        e->capacity = e->capacity ? e->capacity * 2 : 4;
        e->moves    = realloc(e->moves, (size_t)e->capacity * sizeof(move_t));
    }
    e->moves[e->count++] = m;
}

/// @brief Parse a line of play in algebraic (UCI) format and create book entries from it.
/// @param self Opening book.
/// @param line String containing a sequence of moves e.g. "e2e4 e7e5"
/// @return True if line was parsed, false if an illegal move was detected.
static bool parse_line_of_play(opening_book_t *self, char *line)
{
    position_t position = position_from_string(START_FEN);
    char      *save_ptr;
    for (const char *token = strtok_r(line, " \t\r\n", &save_ptr); token; token = strtok_r(NULL, " \t\r\n", &save_ptr))
    {
        if (token[0] == '#')
        {
            return true;
        }
        const zobrist_t   hash  = position.hash;
        const move_list_t legal = position_generate_legal_moves(&position);
        move_t            found = move_none();
        char              move_string[8];
        for (int i = 0; i < legal.size; ++i)
        {
            move_to_string(legal.items[i], move_string, sizeof(move_string));
            if (strcmp(move_string, token) == 0)
            {
                found = legal.items[i];
                break;
            }
        }
        if (move_equals(found, move_none()))
        {
            printf("Error found in book line move %s.\n", token);
            return false;
        }
        book_entry_t *entry = find_entry(self, hash);
        if (!entry)
        {
            entry = create_entry(self, hash);
        }
        entry_push_move(entry, found);
        position_t next;
        position_make_move(&next, &position, found);
        position = next;
    }
    return true;
}

void opening_book_init(opening_book_t *self)
{
    self->entries  = NULL;
    self->count    = 0;
    self->capacity = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    self->prng_state = (uint64_t)ts.tv_nsec ^ ((uint64_t)ts.tv_sec << 32);
    if (!self->prng_state)
    {
        self->prng_state = 1;
    }
}

bool opening_book_from_file(opening_book_t *self, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        return false;
    }
    char line[1024];
    bool ok = true;
    while (fgets(line, sizeof(line), f))
    {
        if (!parse_line_of_play(self, line))
        {
            ok = false;
            break;
        }
    }
    fclose(f);
    return ok;
}

move_t opening_book_get_move(opening_book_t *self, zobrist_t hash)
{
    const book_entry_t *entry = find_entry(self, hash);
    if (entry && entry->count > 0)
    {
        return entry->moves[xorshift64(&self->prng_state) % entry->count];
    }
    return move_none();
}

void opening_book_display_available_moves(const opening_book_t *self, const position_t *position)
{
    const book_entry_t *entry = find_entry(self, position->hash);
    if (!entry)
    {
        return;
    }
    struct seen
    {
        move_t move;
        int    count;
    } seen[256];
    int num_seen = 0;
    for (int i = 0; i < entry->count; ++i)
    {
        move_t move      = entry->moves[i];
        bool   was_found = false;
        for (int j = 0; j < num_seen; ++j)
        {
            if (move_equals(seen[j].move, move))
            {
                ++seen[j].count;
                was_found = true;
                break;
            }
        }
        if (!was_found && num_seen < 256)
        {
            // First time we have seen this move.
            seen[num_seen++] = (struct seen){move, 1};
        }
    }
    printf("MOVE    COUNT\n");
    char buf[8];
    for (int i = 0; i < num_seen; ++i)
    {
        move_to_string(seen[i].move, buf, sizeof(buf));
        printf("%-8s  %3d\n", buf, seen[i].count);
    }
}

void opening_book_free(opening_book_t *self)
{
    for (int i = 0; i < self->count; ++i)
    {
        free(self->entries[i].moves);
    }
    free(self->entries);
    self->entries  = NULL;
    self->count    = 0;
    self->capacity = 0;
}
