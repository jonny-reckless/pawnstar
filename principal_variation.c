#include "pawnstar.h"
#include <Windows.h>
/******************************************************************************
Functions to record and subsequently retrieve the principal variation (PV)
Uses Robert Hyatt's XOR trick to do lockless thread safe retrieval
*******************************************************************************/
typedef struct
{
    uint64  hash;   
    uint64  payload; /* (move | (random << 32)) */
} PvMove;

static PvMove  pvTable[PV_TABLE_SIZE];

/******************************************************************************
Initialize the PV hashtable, deleting any existing entries
*******************************************************************************/
void InitializePrincipalVariationTable(void)
{
    memset(pvTable, 0, sizeof(pvTable)); 
}
/******************************************************************************
Record a new PV move (thread safe)
*******************************************************************************/
void RecordPrincipalVariationMove(uint64 hash, int move)
{ 
        PvMove entry;
        entry.payload = (uint64)move | ((uint64)NextRandom() << 32);
        entry.hash    = hash ^ entry.payload;
        pvTable[hash % PV_TABLE_SIZE] = entry;
}
/******************************************************************************
Get the PV from the PV hashtable, given the root position Zobrist hash
*******************************************************************************/
void FindPrincipalVariation(const Position* rootPosition, int principalVariation[])
{
    const int* const initialPtr = principalVariation;
    int move;
    Position srcPosition[1];
    Position dstPosition[1];
    *srcPosition = *rootPosition;   
    *principalVariation = 0;
    while ((move = GetPrincipalVariationMove(srcPosition->hash)) != 0)
    {
        if (FindMove(initialPtr, move))
        {
            break; // don't want loops in PV
        }
        *principalVariation++ = move;
        *principalVariation   = 0;
        MakeMove(dstPosition, srcPosition, move);
        *srcPosition = *dstPosition;
        if (principalVariation - initialPtr == MAX_PLY - 1)
        {
            break;
        }
    }
}
/******************************************************************************
Retrieve the PV move for the given hash (if any) 
*******************************************************************************/
int GetPrincipalVariationMove(uint64 hash)
{
    const PvMove entry = pvTable[hash % PV_TABLE_SIZE];
    if ((entry.hash ^ entry.payload) == hash)
    {
        return (int)entry.payload;
    }
    return 0;
}
