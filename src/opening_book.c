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

// ---------------------------------------------------------------------------
// xorshift64 PRNG
// ---------------------------------------------------------------------------

static inline uint64_t xorshift64(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return (*s = x);
}

// ---------------------------------------------------------------------------
// book_entry_t helpers
// ---------------------------------------------------------------------------

static int compare_entries(const void *a, const void *b)
{
    const book_entry_t *ea = (const book_entry_t *)a;
    const book_entry_t *eb = (const book_entry_t *)b;
    if (ea->key < eb->key)
    {
        return -1;
    }
    if (ea->key > eb->key)
    {
        return 1;
    }
    return 0;
}

static const book_entry_t *find_entry_const(const opening_book_t *self, zobrist_t key)
{
    book_entry_t probe = {key, NULL, 0, 0};
    return (const book_entry_t *)bsearch(&probe, self->entries, self->entry_count, sizeof(book_entry_t),
                                         compare_entries);
}

/// Get or create a book_entry_t for the given key. Table must be re-sorted after all insertions.
static book_entry_t *get_or_create_entry(opening_book_t *self, zobrist_t key)
{
    if (self->entry_count == self->entry_capacity)
    {
        self->entry_capacity = self->entry_capacity ? self->entry_capacity * 2 : 64;
        self->entries        = realloc(self->entries, self->entry_capacity * sizeof(book_entry_t));
    }
    book_entry_t *e = &self->entries[self->entry_count++];
    e->key          = key;
    e->moves        = NULL;
    e->move_count   = 0;
    e->move_cap     = 0;
    return e;
}

static void entry_push_move(book_entry_t *e, move_t m)
{
    if (e->move_count == e->move_cap)
    {
        e->move_cap = e->move_cap ? e->move_cap * 2 : 4;
        e->moves    = realloc(e->moves, (size_t)e->move_cap * sizeof(move_t));
    }
    e->moves[e->move_count++] = m;
}

// ---------------------------------------------------------------------------
// SAN parser
// ---------------------------------------------------------------------------

static move_t parse_san(const position_t *pos, const char *san_in)
{
    char san[32];
    strncpy(san, san_in, sizeof(san) - 1);
    san[sizeof(san) - 1] = '\0';

    // Strip trailing annotation characters.
    int len = (int)strlen(san);
    while (len > 0 && (san[len - 1] == '+' || san[len - 1] == '#' || san[len - 1] == '!' || san[len - 1] == '?'))
    {
        san[--len] = '\0';
    }

    if (len == 0)
    {
        return move_none();
    }

    const move_list_t moves = position_generate_legal_moves(pos);

    // Castling — O-O-O must be tested before O-O.
    if (strcmp(san, "O-O-O") == 0 || strcmp(san, "0-0-0") == 0)
    {
        for (int i = 0; i < moves.size; ++i)
        {
            move_t m = moves.items[i];
            if (move_type(m) == MOVE_CASTLING && square_file(move_to(m)) == 2)
            {
                return m;
            }
        }
        return move_none();
    }
    if (strcmp(san, "O-O") == 0 || strcmp(san, "0-0") == 0)
    {
        for (int i = 0; i < moves.size; ++i)
        {
            move_t m = moves.items[i];
            if (move_type(m) == MOVE_CASTLING && square_file(move_to(m)) == 6)
            {
                return m;
            }
        }
        return move_none();
    }

    // Determine moving piece from uppercase prefix.
    piece_t moving_piece = PAWN;
    size_t  i            = 0;
    if (isupper((unsigned char)san[0]))
    {
        switch (san[0])
        {
        case 'N':
            moving_piece = KNIGHT;
            break;
        case 'B':
            moving_piece = BISHOP;
            break;
        case 'R':
            moving_piece = ROOK;
            break;
        case 'Q':
            moving_piece = QUEEN;
            break;
        case 'K':
            moving_piece = KING;
            break;
        default:
            return move_none();
        }
        i = 1;
    }

    // Detect promotion piece ("=Q" style).
    piece_t promo_piece = NONE;
    char   *eq          = strchr(san + i, '=');
    if (eq && eq[1])
    {
        switch (eq[1])
        {
        case 'Q':
            promo_piece = QUEEN;
            break;
        case 'R':
            promo_piece = ROOK;
            break;
        case 'B':
            promo_piece = BISHOP;
            break;
        case 'N':
            promo_piece = KNIGHT;
            break;
        default:
            return move_none();
        }
        *eq = '\0'; // Drop "=X" suffix.
        len = (int)strlen(san);
    }

    // Destination square is the last two characters.
    if (len < (int)i + 2)
    {
        return move_none();
    }

    const char dest_file_char = san[len - 2];
    const char dest_rank_char = san[len - 1];
    if (dest_file_char < 'a' || dest_file_char > 'h' || dest_rank_char < '1' || dest_rank_char > '8')
    {
        return move_none();
    }

    square_t to_sq = square_from_coords(dest_file_char - 'a', dest_rank_char - '1');

    // Disambiguation between piece prefix and destination.
    int disambig_file = -1;
    int disambig_rank = -1;
    for (size_t j = i; (int)j + 2 < len; ++j)
    {
        char c = san[j];
        if (c >= 'a' && c <= 'h')
        {
            disambig_file = c - 'a';
        }
        else if (c >= '1' && c <= '8')
        {
            disambig_rank = c - '1';
        }
    }

    for (int k = 0; k < moves.size; ++k)
    {
        move_t m = moves.items[k];
        if (move_piece(m) != moving_piece)
        {
            continue;
        }
        if (move_to(m) != to_sq)
        {
            continue;
        }
        if (disambig_file >= 0 && square_file(move_from(m)) != (uint8_t)disambig_file)
        {
            continue;
        }
        if (disambig_rank >= 0 && square_rank(move_from(m)) != (uint8_t)disambig_rank)
        {
            continue;
        }
        if (promo_piece != NONE && move_promoted(m) != promo_piece)
        {
            continue;
        }
        if (promo_piece == NONE && move_promoted(m) != NONE)
        {
            continue;
        }
        return m;
    }
    return move_none();
}

// ---------------------------------------------------------------------------
// Line-of-play parser
// ---------------------------------------------------------------------------

static bool parse_line_of_play(opening_book_t *self, const char *line)
{
    position_t position = position_from_string(START_FEN);

    char buf[4096];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, " \t\r\n");
    while (token)
    {
        if (strlen(token) == 0 || isdigit((unsigned char)token[0]))
        {
            token = strtok(NULL, " \t\r\n");
            continue;
        }
        if (token[0] == '#')
        {
            return true;
        }

        const zobrist_t hash  = position.hash;
        move_list_t     legal = position_generate_legal_moves(&position);
        move_t          found = move_none();
        char            move_buf[8];
        for (int i = 0; i < legal.size; ++i)
        {
            move_to_string(legal.items[i], move_buf, sizeof(move_buf));
            if (strcmp(move_buf, token) == 0)
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

        // Find or create entry (table is unsorted during construction — linear scan).
        book_entry_t *entry = NULL;
        for (size_t e = 0; e < self->entry_count; ++e)
        {
            if (self->entries[e].key == hash)
            {
                entry = &self->entries[e];
                break;
            }
        }
        if (!entry)
        {
            entry = get_or_create_entry(self, hash);
        }
        entry_push_move(entry, found);

        move_undo_t undo;
        position_make_move(&position, found, &undo);
        token    = strtok(NULL, " \t\r\n");
    }
    return true;
}

// ---------------------------------------------------------------------------
// PGN state
// ---------------------------------------------------------------------------

typedef struct
{
    opening_book_t *book;
    position_t      position;
    int             var_depth;
    bool            in_error;
    char            token[256];
    int             token_len;
} pgn_state_t;

static void pgn_skip_through(FILE *f, char end)
{
    int c;
    while ((c = fgetc(f)) != EOF && c != (int)end)
    {
        ;
    }
}

static void pgn_skip_to_eol(FILE *f)
{
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n')
    {
        ;
    }
}

static void pgn_process_token(pgn_state_t *s)
{
    if (s->token_len == 0)
    {
        return;
    }
    s->token[s->token_len] = '\0';

    if (strcmp(s->token, "1-0") == 0 || strcmp(s->token, "0-1") == 0 || strcmp(s->token, "1/2-1/2") == 0 ||
        strcmp(s->token, "*") == 0)
    {
        s->position  = position_from_string(START_FEN);
        s->in_error  = false;
        s->token_len = 0;
        return;
    }

    if (s->in_error)
    {
        s->token_len = 0;
        return;
    }

    // Strip leading move-number prefix (digits and dots).
    const char *san = s->token;
    while (*san && (isdigit((unsigned char)*san) || *san == '.'))
    {
        ++san;
    }

    if (*san == '\0' || *san == '$')
    {
        s->token_len = 0;
        return;
    }

    const char first        = san[0];
    const bool is_san_token = (first >= 'a' && first <= 'h') || first == 'N' || first == 'B' || first == 'R' ||
                              first == 'Q' || first == 'K' || first == 'O';
    if (!is_san_token)
    {
        s->token_len = 0;
        return;
    }

    const move_t move = parse_san(&s->position, san);
    if (move_equals(move, move_none()))
    {
        printf("PGN parse error: unrecognised move '%s'\n", san);
        s->in_error  = true;
        s->token_len = 0;
        return;
    }

    const zobrist_t hash = s->position.hash;
    // Linear scan during construction (sorted after).
    book_entry_t *entry = NULL;
    for (size_t e = 0; e < s->book->entry_count; ++e)
    {
        if (s->book->entries[e].key == hash)
        {
            entry = &s->book->entries[e];
            break;
        }
    }
    if (!entry)
    {
        entry = get_or_create_entry(s->book, hash);
    }
    entry_push_move(entry, move);

    move_undo_t undo;
    position_make_move(&s->position, move, &undo);
    s->token_len = 0;
}

static bool parse_pgn(opening_book_t *self, FILE *f)
{

    pgn_state_t s;
    s.book      = self;
    s.position  = position_from_string(START_FEN);
    s.var_depth = 0;
    s.in_error  = false;
    s.token_len = 0;

    int c;
    while ((c = fgetc(f)) != EOF)
    {
        if (c == '{')
        {
            pgn_process_token(&s);
            pgn_skip_through(f, '}');
            continue;
        }
        if (c == ';')
        {
            pgn_process_token(&s);
            pgn_skip_to_eol(f);
            continue;
        }
        if (c == '(')
        {
            pgn_process_token(&s);
            ++s.var_depth;
            continue;
        }
        if (c == ')')
        {
            if (s.var_depth > 0)
            {
                --s.var_depth;
            }
            continue;
        }
        if (s.var_depth > 0)
        {
            continue;
        }
        if (c == '[')
        {
            pgn_process_token(&s);
            pgn_skip_through(f, ']');
            continue;
        }
        if (isspace(c))
        {
            pgn_process_token(&s);
            continue;
        }

        if (s.token_len < (int)(sizeof(s.token) - 1))
        {
            s.token[s.token_len++] = (char)c;
        }
    }
    pgn_process_token(&s);
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void opening_book_init(opening_book_t *self)
{
    self->entries        = NULL;
    self->entry_count    = 0;
    self->entry_capacity = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    self->prng_state = (uint64_t)ts.tv_nsec ^ ((uint64_t)ts.tv_sec << 32);
    if (!self->prng_state)
    {
        self->prng_state = 1;
    }
}

bool opening_book_initialize(opening_book_t *self, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        return false;
    }

    // Peek at the first non-whitespace character to detect format.
    int c;
    while ((c = fgetc(f)) != EOF && isspace(c))
    {
        ;
    }

    bool ok;
    if (c == '[')
    {
        // PGN format — put character back isn't portable for all FILE*, so we just reset.
        rewind(f);
        // Skip whitespace again.
        while ((c = fgetc(f)) != EOF && isspace(c))
        {
            ;
        }
        // Now c is '[': proceed with PGN parser reading from f.
        // Push the '[' back by unreading it.
        ungetc(c, f);
        ok = parse_pgn(self, f);
    }
    else if (c == EOF)
    {
        fclose(f);
        return true;
    }
    else
    {
        // Line-of-play format: rewind and parse line-by-line.
        rewind(f);
        char line[4096];
        ok = true;
        while (fgets(line, sizeof(line), f))
        {
            if (!parse_line_of_play(self, line))
            {
                ok = false;
                break;
            }
        }
    }

    fclose(f);

    // Sort entries by key for bsearch.
    if (self->entry_count > 0)
    {
        qsort(self->entries, self->entry_count, sizeof(book_entry_t), compare_entries);
    }

    return ok;
}

move_t opening_book_get_move(opening_book_t *self, zobrist_t hash)
{
    const book_entry_t *entry = find_entry_const(self, hash);
    if (entry && entry->move_count > 0)
    {
        return entry->moves[(int)(xorshift64(&self->prng_state) % (uint64_t)entry->move_count)];
    }
    return move_none();
}

void opening_book_display_available_moves(const opening_book_t *self, const position_t *position)
{
    const book_entry_t *entry = find_entry_const(self, position->hash);
    if (!entry)
    {
        return;
    }

    // Count occurrences of each move via a simple parallel array.
    move_t seen_moves[256];
    int    seen_counts[256];
    int    seen_n = 0;
    for (int i = 0; i < entry->move_count; ++i)
    {
        move_t m     = entry->moves[i];
        bool   found = false;
        for (int j = 0; j < seen_n; ++j)
        {
            if (move_equals(seen_moves[j], m))
            {
                ++seen_counts[j];
                found = true;
                break;
            }
        }
        if (!found && seen_n < 256)
        {
            seen_moves[seen_n]  = m;
            seen_counts[seen_n] = 1;
            ++seen_n;
        }
    }

    printf("MOVE   COUNT\n");
    char buf[8];
    for (int i = 0; i < seen_n; ++i)
    {
        move_to_string(seen_moves[i], buf, sizeof(buf));
        printf("%-8s %3d\n", buf, seen_counts[i]);
    }
}

void opening_book_free(opening_book_t *self)
{
    for (size_t i = 0; i < self->entry_count; ++i)
    {
        free(self->entries[i].moves);
    }
    free(self->entries);
    self->entries        = NULL;
    self->entry_count    = 0;
    self->entry_capacity = 0;
}
