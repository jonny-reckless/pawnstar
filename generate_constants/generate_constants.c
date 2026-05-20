/// @file Precomputes constants used by pawnstar.
/// Compiled and executed by the Makefile; output is redirected to generated_data.c.
#include <immintrin.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

typedef enum
{
    DIR_NORTH = 0,
    DIR_NORTHEAST,
    DIR_EAST,
    DIR_SOUTHEAST,
    DIR_SOUTH,
    DIR_SOUTHWEST,
    DIR_WEST,
    DIR_NORTHWEST
} direction_t;

typedef uint64_t (*shift_fn_t)(uint64_t);
typedef uint64_t (*mask_fn_t)(uint8_t);
typedef uint64_t (*attack_fn_t)(uint64_t, uint8_t);
typedef uint64_t (*generator_fn_t)(uint8_t);

// ---------------------------------------------------------------------------
// PRNG (xorshift64, same seed as original mt19937_64)
// ---------------------------------------------------------------------------

static uint64_t prng_state = 0xAA55AA55AA55AA55ull;
static uint64_t prng_next(void)
{
    uint64_t x = prng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return (prng_state = x);
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static const uint64_t GEN_FILE_A = 0x0101010101010101ull;
static const uint64_t GEN_FILE_H = 0x8080808080808080ull;

static const char *PIECE_NAMES[] = {"none", "pawn", "knight", "bishop", "rook", "queen", "king"};
static const char *COLOR_NAMES[] = {"white", "black"};

// ---------------------------------------------------------------------------
// square_t / bitboard helpers (generator-internal; uint64_t not bitboard_t struct)
// ---------------------------------------------------------------------------

static uint8_t file_of(uint8_t sq)
{
    return sq & 7;
}
static uint8_t rank_of(uint8_t sq)
{
    return sq >> 3;
}

static void square_name(uint8_t sq, char *buf) /* buf >= 3 */
{
    buf[0] = (char)('a' + file_of(sq));
    buf[1] = (char)('1' + rank_of(sq));
    buf[2] = '\0';
}

static void square_name_upper(uint8_t sq, char *buf) /* buf >= 3 */
{
    buf[0] = (char)('A' + file_of(sq));
    buf[1] = (char)('1' + rank_of(sq));
    buf[2] = '\0';
}

static uint64_t sq_bb(uint8_t sq)
{
    return 1ull << sq;
}
static uint64_t coord_bb(int x, int y)
{
    return 1ull << (x + 8 * y);
}
static int is_in_board(int x, int y)
{
    return x == (x & 7) && y == (y & 7);
}

static uint64_t shift_north(uint64_t b)
{
    return b << 8;
}
static uint64_t shift_northeast(uint64_t b)
{
    return (b & ~GEN_FILE_H) << 9;
}
static uint64_t shift_east(uint64_t b)
{
    return (b & ~GEN_FILE_H) << 1;
}
static uint64_t shift_southeast(uint64_t b)
{
    return (b & ~GEN_FILE_H) >> 7;
}
static uint64_t shift_south(uint64_t b)
{
    return b >> 8;
}
static uint64_t shift_southwest(uint64_t b)
{
    return (b & ~GEN_FILE_A) >> 9;
}
static uint64_t shift_west(uint64_t b)
{
    return (b & ~GEN_FILE_A) >> 1;
}
static uint64_t shift_northwest(uint64_t b)
{
    return (b & ~GEN_FILE_A) << 7;
}

static const shift_fn_t SHIFT_FUNCTIONS[8] = {shift_north, shift_northeast, shift_east, shift_southeast,
                                              shift_south, shift_southwest, shift_west, shift_northwest};

// ---------------------------------------------------------------------------
// Dynamic uint64 array (replaces std::vector<uint64_t>)
// ---------------------------------------------------------------------------

typedef struct
{
    uint64_t *data;
    size_t    size;
    size_t    cap;
} u64_vec_t;
typedef struct
{
    uint8_t *data;
    size_t   size;
    size_t   cap;
} u8_vec_t;

static void u64_vec_push(u64_vec_t *v, uint64_t x)
{
    if (v->size >= v->cap)
    {
        v->cap  = v->cap ? v->cap * 2 : 64;
        v->data = realloc(v->data, v->cap * sizeof(uint64_t));
    }
    v->data[v->size++] = x;
}
static void u64_vec_free(u64_vec_t *v)
{
    free(v->data);
    v->data = NULL;
    v->size = v->cap = 0;
}

static void u8_vec_push(u8_vec_t *v, uint8_t x)
{
    if (v->size >= v->cap)
    {
        v->cap  = v->cap ? v->cap * 2 : 64;
        v->data = realloc(v->data, v->cap * sizeof(uint8_t));
    }
    v->data[v->size++] = x;
}
static void u8_vec_free(u8_vec_t *v)
{
    free(v->data);
    v->data = NULL;
    v->size = v->cap = 0;
}

// ---------------------------------------------------------------------------
// generator_t functions for bitboard tables
// ---------------------------------------------------------------------------

static uint64_t ray_from(uint8_t sq, direction_t direction)
{
    uint64_t   result = 0;
    shift_fn_t fn     = SHIFT_FUNCTIONS[direction];
    for (uint64_t b = fn(sq_bb(sq)); b; b = fn(b))
        result |= b;
    return result;
}

static uint64_t knight_attacks(uint8_t sq)
{
    static const int DX[8]  = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int DY[8]  = {-1, 1, -2, 2, -2, 2, -1, 1};
    uint64_t         result = 0;
    for (int i = 0; i < 8; i++)
    {
        int x = file_of(sq) + DX[i];
        int y = rank_of(sq) + DY[i];
        if (is_in_board(x, y))
            result |= coord_bb(x, y);
    }
    return result;
}

static uint64_t bishop_attacks_empty(uint8_t sq)
{
    return ray_from(sq, DIR_NORTHEAST) | ray_from(sq, DIR_NORTHWEST) | ray_from(sq, DIR_SOUTHEAST) |
           ray_from(sq, DIR_SOUTHWEST);
}

static uint64_t rook_attacks_empty(uint8_t sq)
{
    return ray_from(sq, DIR_NORTH) | ray_from(sq, DIR_SOUTH) | ray_from(sq, DIR_EAST) | ray_from(sq, DIR_WEST);
}

static uint64_t queen_attacks_empty(uint8_t sq)
{
    return bishop_attacks_empty(sq) | rook_attacks_empty(sq);
}

static uint64_t king_fill(uint64_t b)
{
    return shift_north(b) | shift_northeast(b) | shift_east(b) | shift_southeast(b) | shift_south(b) |
           shift_southwest(b) | shift_west(b) | shift_northwest(b);
}

static uint64_t king_attacks(uint8_t sq)
{
    return king_fill(sq_bb(sq));
}
static uint64_t king_attacks2(uint8_t sq)
{
    return king_fill(king_fill(sq_bb(sq))) & ~sq_bb(sq);
}

static uint64_t king_pawn_shelter_white(uint8_t sq)
{
    uint64_t b = sq_bb(sq);
    return sq == 4 ? 0 : shift_northwest(b) | shift_north(b) | shift_northeast(b);
}

static uint64_t king_pawn_shelter_black(uint8_t sq)
{
    uint64_t b = sq_bb(sq);
    return sq == 60 ? 0 : shift_southwest(b) | shift_south(b) | shift_southeast(b);
}

static uint64_t passed_pawn_mask_white(uint8_t sq)
{
    uint64_t b = ray_from(sq, DIR_NORTH);
    return b | shift_west(b) | shift_east(b);
}

static uint64_t passed_pawn_mask_black(uint8_t sq)
{
    uint64_t b = ray_from(sq, DIR_SOUTH);
    return b | shift_west(b) | shift_east(b);
}

static uint64_t isolated_pawn_mask_white(uint8_t sq)
{
    uint64_t b = GEN_FILE_A << file_of(sq);
    return shift_west(b) | shift_east(b);
}

static uint64_t isolated_pawn_mask_black(uint8_t sq)
{
    return isolated_pawn_mask_white(sq);
}

static uint64_t supported_pawn_mask_white(uint8_t sq)
{
    uint64_t b = ray_from(sq, DIR_SOUTH) | sq_bb(sq);
    return shift_west(b) | shift_east(b);
}

static uint64_t supported_pawn_mask_black(uint8_t sq)
{
    uint64_t b = ray_from(sq, DIR_NORTH) | sq_bb(sq);
    return shift_west(b) | shift_east(b);
}

static uint64_t doubled_pawn_mask_white(uint8_t sq)
{
    return ray_from(sq, DIR_NORTH);
}
static uint64_t doubled_pawn_mask_black(uint8_t sq)
{
    return ray_from(sq, DIR_SOUTH);
}

static uint64_t pawn_attacks_white(uint8_t sq)
{
    return shift_northwest(sq_bb(sq)) | shift_northeast(sq_bb(sq));
}
static uint64_t pawn_attacks_black(uint8_t sq)
{
    return shift_southwest(sq_bb(sq)) | shift_southeast(sq_bb(sq));
}

static uint64_t gen_north(uint8_t sq)
{
    return ray_from(sq, DIR_NORTH);
}
static uint64_t gen_northeast(uint8_t sq)
{
    return ray_from(sq, DIR_NORTHEAST);
}
static uint64_t gen_east(uint8_t sq)
{
    return ray_from(sq, DIR_EAST);
}
static uint64_t gen_southeast(uint8_t sq)
{
    return ray_from(sq, DIR_SOUTHEAST);
}
static uint64_t gen_south(uint8_t sq)
{
    return ray_from(sq, DIR_SOUTH);
}
static uint64_t gen_southwest(uint8_t sq)
{
    return ray_from(sq, DIR_SOUTHWEST);
}
static uint64_t gen_west(uint8_t sq)
{
    return ray_from(sq, DIR_WEST);
}
static uint64_t gen_northwest(uint8_t sq)
{
    return ray_from(sq, DIR_NORTHWEST);
}

static uint64_t intervening_squares(uint8_t from, uint8_t to)
{
    static const direction_t DIRECTIONS[8] = {DIR_NORTH, DIR_NORTHEAST, DIR_EAST, DIR_SOUTHEAST,
                                              DIR_SOUTH, DIR_SOUTHWEST, DIR_WEST, DIR_NORTHWEST};
    for (int i = 0; i < 8; i++)
    {
        uint64_t ray = ray_from(from, DIRECTIONS[i]);
        if (ray & sq_bb(to))
            return ray ^ ray_from(to, DIRECTIONS[i]) ^ sq_bb(to);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// generator_t entry table
// ---------------------------------------------------------------------------

typedef struct
{
    const char    *name;
    generator_fn_t function;
} generator_t;

static const generator_t BITBOARD_GENERATORS[] = {
    {"NORTH", gen_north},
    {"NORTHEAST", gen_northeast},
    {"EAST", gen_east},
    {"SOUTHEAST", gen_southeast},
    {"SOUTH", gen_south},
    {"SOUTHWEST", gen_southwest},
    {"WEST", gen_west},
    {"NORTHWEST", gen_northwest},
    {"PAWN_ATTACKS_WHITE", pawn_attacks_white},
    {"PAWN_ATTACKS_BLACK", pawn_attacks_black},
    {"KNIGHT_ATTACKS", knight_attacks},
    {"BISHOP_ATTACKS", bishop_attacks_empty},
    {"ROOK_ATTACKS", rook_attacks_empty},
    {"QUEEN_ATTACKS", queen_attacks_empty},
    {"KING_ATTACKS", king_attacks},
    {"KING_ATTACKS2", king_attacks2},
    {"KING_PAWN_SHELTER_WHITE", king_pawn_shelter_white},
    {"KING_PAWN_SHELTER_BLACK", king_pawn_shelter_black},
    {"PASSED_PAWN_MASK_WHITE", passed_pawn_mask_white},
    {"ISOLATED_PAWN_MASK_WHITE", isolated_pawn_mask_white},
    {"SUPPORTED_PAWN_MASK_WHITE", supported_pawn_mask_white},
    {"DOUBLED_PAWN_MASK_WHITE", doubled_pawn_mask_white},
    {"PASSED_PAWN_MASK_BLACK", passed_pawn_mask_black},
    {"ISOLATED_PAWN_MASK_BLACK", isolated_pawn_mask_black},
    {"SUPPORTED_PAWN_MASK_BLACK", supported_pawn_mask_black},
    {"DOUBLED_PAWN_MASK_BLACK", doubled_pawn_mask_black},
};
static const int NUM_BITBOARD_GENERATORS = (int)(sizeof(BITBOARD_GENERATORS) / sizeof(BITBOARD_GENERATORS[0]));

// ---------------------------------------------------------------------------
// Pext computation
// ---------------------------------------------------------------------------

static uint64_t ray_occupancy_mask(uint8_t sq, direction_t dir)
{
    uint64_t   result = 0, last = 0;
    shift_fn_t fn = SHIFT_FUNCTIONS[dir];
    for (uint64_t b = fn(sq_bb(sq)); b; b = fn(b))
        result |= b, last = b;
    return result ^ last;
}

static uint64_t bishop_occupancy_mask(uint8_t sq)
{
    return ray_occupancy_mask(sq, DIR_NORTHEAST) | ray_occupancy_mask(sq, DIR_NORTHWEST) |
           ray_occupancy_mask(sq, DIR_SOUTHEAST) | ray_occupancy_mask(sq, DIR_SOUTHWEST);
}

static uint64_t rook_occupancy_mask(uint8_t sq)
{
    return ray_occupancy_mask(sq, DIR_NORTH) | ray_occupancy_mask(sq, DIR_SOUTH) | ray_occupancy_mask(sq, DIR_EAST) |
           ray_occupancy_mask(sq, DIR_WEST);
}

static uint64_t ray_attacks(uint64_t occ, uint8_t sq, direction_t dir)
{
    uint64_t   result = 0;
    shift_fn_t fn     = SHIFT_FUNCTIONS[dir];
    for (uint64_t b = fn(sq_bb(sq)); b; b = fn(b))
    {
        result |= b;
        if (b & occ)
            break;
    }
    return result;
}

static uint64_t bishop_attacks(uint64_t occ, uint8_t sq)
{
    return ray_attacks(occ, sq, DIR_NORTHEAST) | ray_attacks(occ, sq, DIR_SOUTHEAST) |
           ray_attacks(occ, sq, DIR_SOUTHWEST) | ray_attacks(occ, sq, DIR_NORTHWEST);
}

static uint64_t rook_attacks(uint64_t occ, uint8_t sq)
{
    return ray_attacks(occ, sq, DIR_NORTH) | ray_attacks(occ, sq, DIR_EAST) | ray_attacks(occ, sq, DIR_SOUTH) |
           ray_attacks(occ, sq, DIR_WEST);
}

static int compare_u64(const void *a, const void *b)
{
    uint64_t ua = *(const uint64_t *)a, ub = *(const uint64_t *)b;
    return (ua > ub) - (ua < ub);
}

typedef struct
{
    uint64_t  mask;
    u64_vec_t attacks; // unique attack sets
    u8_vec_t  indices; // index per occupancy combination
} gen_pext_t;

static gen_pext_t compute_pext(uint8_t sq, mask_fn_t mask_fn, attack_fn_t attack_fn)
{
    gen_pext_t p = {0};
    p.mask       = mask_fn(sq);

    // Enumerate all occupancy combinations from the mask.
    u64_vec_t occupancies = {0};
    uint64_t  n           = 0;
    do
    {
        u64_vec_push(&occupancies, n);
        n = (n - 1) & p.mask;
    } while (n);

    // Compute attack set for each occupancy.
    u64_vec_t all_attacks = {0};
    for (size_t i = 0; i < occupancies.size; i++)
    {
        uint64_t idx = _pext_u64(occupancies.data[i], p.mask);
        while (all_attacks.size <= idx)
            u64_vec_push(&all_attacks, 0);
        all_attacks.data[idx] = attack_fn(occupancies.data[i], sq);
    }

    // Build sorted unique attacks list.
    u64_vec_t unique = {0};
    for (size_t i = 0; i < all_attacks.size; i++)
        u64_vec_push(&unique, all_attacks.data[i]);
    qsort(unique.data, unique.size, sizeof(uint64_t), compare_u64);
    // Remove duplicates.
    size_t u = 0;
    for (size_t i = 0; i < unique.size; i++)
        if (i == 0 || unique.data[i] != unique.data[i - 1])
            unique.data[u++] = unique.data[i];
    unique.size = u;

    // Build indices: for each occupancy, find the index of its attack in unique[].
    for (size_t i = 0; i < all_attacks.size; i++)
    {
        uint64_t target = all_attacks.data[i];
        uint8_t  found  = 0;
        for (size_t j = 0; j < unique.size; j++)
            if (unique.data[j] == target)
            {
                found = (uint8_t)j;
                break;
            }
        u8_vec_push(&p.indices, found);
    }

    p.attacks = unique;
    u64_vec_free(&all_attacks);
    u64_vec_free(&occupancies);
    return p;
}

// ---------------------------------------------------------------------------
// Output routines
// ---------------------------------------------------------------------------

static void generate_bitboards(void)
{
    char sqname[3];
    for (int g = 0; g < NUM_BITBOARD_GENERATORS; g++)
    {
        printf("const bitboard_t %s[64] =\n{", BITBOARD_GENERATORS[g].name);
        for (uint8_t sq = 0; sq < 64; sq++)
        {
            uint64_t b = BITBOARD_GENERATORS[g].function(sq);
            square_name(sq, sqname);
            printf("\n    0x%016" PRIX64 "ULL, // %s popcnt %2d", b, sqname, __builtin_popcountll(b));
        }
        printf("\n};\n");
    }
}

static void generate_intervening_squares(void)
{
    char sqname[3];
    printf("const bitboard_t INTERVENING_SQUARES[64][64] =\n{");
    for (uint8_t i = 0; i < 64; i++)
    {
        square_name(i, sqname);
        printf("\n    { // square %s", sqname);
        for (uint8_t j = 0; j < 64; j++)
        {
            if ((j & 7) == 0)
                printf("\n        ");
            printf("0x%016" PRIX64 "ULL,", intervening_squares(i, j));
        }
        printf("\n    },");
    }
    printf("\n};\n");
}

static void generate_hashes(void)
{
    printf("const zobrist_t PIECE_SQUARE_HASHES[2][6][64] =\n{");
    for (int color = 0; color < 2; color++)
    {
        printf("\n    { // %s", COLOR_NAMES[color]);
        for (int piece = 1; piece <= 6; piece++) // PAWN..KING
        {
            printf("\n        { // %s", PIECE_NAMES[piece]);
            for (uint8_t i = 0; i < 64; i++)
            {
                if ((i & 7) == 0)
                    printf("\n            ");
                printf("0x%016" PRIX64 ",", prng_next());
            }
            printf("\n        },");
        }
        printf("\n    },");
    }
    printf("\n};\n");

    printf("const zobrist_t CASTLING_RIGHTS_HASHES[16] =\n{");
    for (uint8_t i = 0; i < 16; i++)
    {
        if ((i & 7) == 0)
            printf("\n    ");
        printf("0x%016" PRIX64 ",", prng_next());
    }
    printf("\n};\n");

    printf("const zobrist_t EN_PASSANT_HASHES[64] =\n{");
    for (uint8_t i = 0; i < 64; i++)
    {
        if ((i & 7) == 0)
            printf("\n    ");
        uint8_t rank = rank_of(i);
        printf("0x%016" PRIX64 ",", (rank == 2 || rank == 5) ? prng_next() : (uint64_t)0);
    }
    printf("\n};\n");
}

typedef struct
{
    const char *name;
    attack_fn_t attack_fn;
    mask_fn_t   mask_fn;
} piece_pext_def_t;

static const piece_pext_def_t PIECE_PEXTS[2] = {
    {"BISHOP", bishop_attacks, bishop_occupancy_mask},
    {"ROOK", rook_attacks, rook_occupancy_mask},
};

static void generate_pext_bitboards(void)
{
    char sqname[3];
    for (int p = 0; p < 2; p++)
    {
        const piece_pext_def_t *piece = &PIECE_PEXTS[p];
        uint64_t                masks[64];

        for (uint8_t sq = 0; sq < 64; sq++)
        {
            gen_pext_t pext = compute_pext(sq, piece->mask_fn, piece->attack_fn);
            masks[sq]       = pext.mask;
            square_name_upper(sq, sqname);

            printf("static const uint8_t %s_PEXT_INDICES_%s[%zu] =\n{", piece->name, sqname, pext.indices.size);
            for (size_t i = 0; i < pext.indices.size; i++)
            {
                if (i % 16 == 0)
                    printf("\n    ");
                printf("0x%02X, ", pext.indices.data[i]);
            }
            printf("\n};\n");

            printf("static const bitboard_t %s_PEXT_ATTACKS_%s[%zu] =\n{", piece->name, sqname, pext.attacks.size);
            for (size_t i = 0; i < pext.attacks.size; i++)
            {
                if (i % 8 == 0)
                    printf("\n    ");
                printf("0x%016" PRIX64 "ULL,", pext.attacks.data[i]);
            }
            printf("\n};\n");

            u64_vec_free(&pext.attacks);
            u8_vec_free(&pext.indices);
        }

        printf("const pext_bitboard_t %s_PEXTS[64] =\n{\n", piece->name);
        for (uint8_t i = 0; i < 64; i++)
        {
            square_name_upper(i, sqname);
            printf("    {\n");
            printf("        .occupancy_mask = 0x%016" PRIX64 "ULL,\n", masks[i]);
            printf("        .attacks        = %s_PEXT_ATTACKS_%s,\n", piece->name, sqname);
            printf("        .indices        = %s_PEXT_INDICES_%s,\n", piece->name, sqname);
            printf("    },\n");
        }
        printf("};\n");
    }
}

int main(void)
{
    printf("// Generated by generate_constants.c on " __DATE__ " at " __TIME__ "\n");
    printf("#include \"generated_data.h\"\n\n");
    generate_bitboards();
    generate_intervening_squares();
    generate_hashes();
    generate_pext_bitboards();
    return 0;
}
