#include "pawnstar.h"

#define MEGABYTE                    0x100000
#define TRANSPOSITIONS_PER_BUCKET   8

static bool IsPrime(int x);

typedef struct HashBucket
{
    long            mutex[1];
    Transposition   transpositions[TRANSPOSITIONS_PER_BUCKET];
} HashBucket;

static HashBucket*  transpositionTable;
static int          tableBucketCount;
/******************************************************************************
Delete the transposition table
NB: NOT thread safe
*******************************************************************************/
void FreeTranspositionTable(void)
{
    if (transpositionTable)
    {
        free(transpositionTable);
        transpositionTable = NULL;
        tableBucketCount   = 0;
    } 
}
/******************************************************************************
Create the transposition table of a specified maximum size
NB: NOT thread safe
*******************************************************************************/
bool InitializeTranspositionTable(int megabytes)
{
    FreeTranspositionTable();
    tableBucketCount = (megabytes * MEGABYTE) / sizeof(HashBucket);
    /* Find the next smallest prime number and make the table that size */
    if ((tableBucketCount & 1) == 0)
    {
        --tableBucketCount;
    }
    while (!IsPrime(tableBucketCount))
    {
        tableBucketCount -= 2;
    }
    transpositionTable = calloc(tableBucketCount, sizeof(HashBucket));
    if (!transpositionTable)
    {
        printf("ERROR: unable to create transposition transpositionTable of %u megabytes\n", megabytes);
        tableBucketCount = 0;
        return false;
    }
    return true;
}
/******************************************************************************
Find a transposition entry for this position if one exists
*******************************************************************************/
bool FindTransposition(uint64 hash, Transposition* transposition)
{
    int i;
    HashBucket* const bucket = &transpositionTable[hash % tableBucketCount]; 
    while (_InterlockedCompareExchange(bucket->mutex, 1, 0) != 0)
    {
        INCREMENT("transposition lock contention retrieve");
    }
    for (i = TRANSPOSITIONS_PER_BUCKET - 1; i >= 0; --i)
    {
        if (bucket->transpositions[i].hash == hash)
        {
            *transposition = bucket->transpositions[i];
            _InterlockedExchange(bucket->mutex, 0);
            return true;
        }
    }
    _InterlockedExchange(bucket->mutex, 0);
    return false;
}
/******************************************************************************
Insert a new entry into the transposition table.
Hash bucket replacement policy (in priority order):
# replace an entry with the same hash
# replace an empty slot
# replace a non PV node with a PV node
# replace the entry with the smallest depth
# replace an entry with no move with one containing a move
*******************************************************************************/
void RecordTransposition(uint64 hash, int depth, int score, int move, int nodeType)
{  
    int i;
    int replace  = 0;  
    int bestScore = 0;
    HashBucket* const bucket = &transpositionTable[hash % tableBucketCount];  
    while (_InterlockedCompareExchange(bucket->mutex, 1, 0) != 0)
    {
        INCREMENT("transposition lock contention record");
    }
    for (i = TRANSPOSITIONS_PER_BUCKET - 1; i >= 0; --i)
    {
        const int score = 
           16 * (bucket->transpositions[i].hash == hash)                                                               +
            8 * (bucket->transpositions[i].hash == 0)                                                                  +
            4 * (bucket->transpositions[replace].nodeType == NODE_PV && bucket->transpositions[i].nodeType != NODE_PV) +
            2 * (bucket->transpositions[i].depth < bucket->transpositions[replace].depth)                              +            
                (bucket->transpositions[i].move == 0);

        if (score > bestScore)
        {
            bestScore = score;
            replace = i;
            if (score >= 16)
            {
                break;
            }
        }
    }
    bucket->transpositions[replace].hash        = hash;
    bucket->transpositions[replace].depth       = (short)depth;
    bucket->transpositions[replace].score       = (short)score;
    bucket->transpositions[replace].move        = move;
    bucket->transpositions[replace].nodeType    = nodeType;
    _InterlockedExchange(bucket->mutex, 0);
}
/******************************************************************************
We get marginally better dispersion when the hashtable size is a prime number
Determine if a candidate size is prime (excluding 1 and 2)
*******************************************************************************/
static bool IsPrime(int x)
{
    int i;
    if (x < 3)
    {
        return false;
    }
    for (i = 2; i * i <= x; ++i)
    {
        if (x % i == 0)
        {
            return false;
        }
    }
    return true;
}