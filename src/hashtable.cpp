/*

    Amy - a chess playing program

    Copyright (c) 2002-2026, Thorsten Greiner
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.

*/

/*
 * hashtable.c - hashtable management routines
 */

#include <stddef.h>
#include <string.h>

#include "hashtable.h"
#include "random.h"
#include "safe_malloc.h"
#include "search.h"
#include "utils.h"

#define HT_AGE (0x3f)
#define HT_NCPU ((0x3f) << 6)
#define HT_NCPU_INCREMENT (1 << 6)
#define HT_THREAT (1 << 12)
#define HT_EXACT (1 << 13)
#define HT_LBOUND (1 << 14)
#define HT_UBOUND (1 << 15)

#define PT_INVALID 0xffff

hash_t HashKeys[2][8][CBitBoard::SIZE];
hash_t HashKeysEP[CBitBoard::SIZE];
hash_t HashKeysCastle[16];
hash_t STMKey;

static int HT_Bits = 17;
static int PT_Bits = 15;
static int ST_Bits = 15;

static unsigned int HT_Size, HT_Mask;
static unsigned int PT_Size, PT_Mask;
static unsigned int ST_Size, ST_Mask;

int L_HT_Bits = 16, L_HT_Size, L_HT_Mask;

static struct HTEntry *TranspositionTable = NULL;
static struct PTEntry *PawnTable = NULL;
static struct STEntry *ScoreTable = NULL;
static int HTGeneration = 0;

static OPTIONAL_ATOMIC unsigned int HTStoreFailed = 0, HTStoreTried = 0;

#if MP

#include <atomic>

#define MUTEX_BITS 8
#define MUTEX_COUNT (1 << MUTEX_BITS)
#define MUTEX_MASK (MUTEX_COUNT - 1)
static std::atomic<int> TranspositionMutex[MUTEX_COUNT];
static std::atomic<int> PawnMutex[MUTEX_COUNT];
static std::atomic<int> ScoreMutex[MUTEX_COUNT];

/**
 * Acquire a read lock for the given pointer. There can be many read locks,
 * but only a single write lock.
 */
static void acquire_read_lock(std::atomic<int> *pData) {
    for (;;) {
        int nVal = pData->load();
        if (nVal >= 0) {
            bool fResult = pData->compare_exchange_strong(nVal, nVal + 1);
            if (fResult)
                return;
        }
    }
}

/**
 * Release the read lock.
 */
static void release_read_lock(std::atomic<int> *pData) {
    for (;;) {
        int nVal = pData->load();
        if (nVal > 0) {
            bool fResult = pData->compare_exchange_strong(nVal, nVal - 1);
            if (fResult)
                return;
        }
    }
}

/**
 * Acquire a write lock for the given pointer. There can be many read locks,
 * but only a single write lock.
 */
static void acquire_write_lock(std::atomic<int> *pData) {
    for (;;) {
        int nVal = pData->load();
        if (nVal == 0) {
            bool fResult = pData->compare_exchange_strong(nVal, -1);
            if (fResult)
                return;
        }
    }
}

/**
 * Release the write lock.
 */
static void release_write_lock(std::atomic<int> *pData) {
    for (;;) {
        int nVal = pData->load();
        if (nVal == -1) {
            bool fResult = pData->compare_exchange_strong(nVal, 0);
            if (fResult)
                return;
        }
    }
}

#endif

/**
 * Gets an entry from the global transposition table.
 */
static inline struct HTEntry GetHTEntry(hash_t qwKey) {
#if MP
    std::atomic<int> *pMutex = TranspositionMutex + ((qwKey >> 32) & MUTEX_MASK);
    acquire_read_lock(pMutex);
#endif /* MP */

    struct HTEntry Entry = TranspositionTable[(qwKey >> 32) & HT_Mask];

#if MP
    release_read_lock(pMutex);
#endif /* MP */

    return Entry;
}

/**
 * Puts an entry to the global transposition table.
 */
static inline void PutHTEntry(hash_t qwKey, struct HTEntry Entry) {
#if MP
    std::atomic<int> *pMutex = TranspositionMutex + ((qwKey >> 32) & MUTEX_MASK);
    acquire_write_lock(pMutex);
#endif /* MP */

    TranspositionTable[(qwKey >> 32) & HT_Mask] = Entry;

#if MP
    release_write_lock(pMutex);
#endif /* MP */
}

/**
 *
 */
static inline bool PutHTEntryBestEffort(hash_t qwKey, struct HTEntry Entry,
                                        int nDepth) {
    const hash_t qwKey1 = qwKey;
    const hash_t qwKey2 = qwKey + 1;

    struct HTEntry Entry1 = GetHTEntry(qwKey1);
    struct HTEntry Entry2 = GetHTEntry(qwKey2);

    /* Overwrite any matching entry. */
    if (Entry1.ht_Signature == (unsigned int)qwKey) {
        PutHTEntry(qwKey1, Entry);
        return true;
    }
    if (Entry2.ht_Signature == (unsigned int)qwKey) {
        PutHTEntry(qwKey2, Entry);
        return true;
    }

    /* Overwrite entries with lower depth. */
    if (Entry1.ht_Depth <= Entry2.ht_Depth) {
        if (Entry1.ht_Depth <= nDepth) {
            PutHTEntry(qwKey1, Entry);
            return true;
        }
        if (Entry2.ht_Depth <= nDepth) {
            PutHTEntry(qwKey2, Entry);
            return true;
        }
    } else {
        if (Entry2.ht_Depth <= nDepth) {
            PutHTEntry(qwKey2, Entry);
            return true;
        }
        if (Entry1.ht_Depth <= nDepth) {
            PutHTEntry(qwKey1, Entry);
            return true;
        }
    }

    /* Overwrite entries from older generation. */
    if ((Entry1.ht_Flags & HT_AGE) != HTGeneration) {
        PutHTEntry(qwKey1, Entry);
        return true;
    }
    if ((Entry2.ht_Flags & HT_AGE) != HTGeneration) {
        PutHTEntry(qwKey2, Entry);
        return true;
    }

    return false;
}

#if MP
LookupResult ProbeHT(hash_t qwKey, int *pScore, int nDepth, CMove *pBestm,
                     bool *pThreat, int nPly, int nExclusiveP,
                     struct HTEntry *pLocalHT)
#else
LookupResult ProbeHT(hash_t qwKey, int *pScore, int nDepth, CMove *pBestm,
                     bool *pThreat, int nPly)
#endif
{
    hash_t qwEffectiveKey = qwKey;
    struct HTEntry h = GetHTEntry(qwEffectiveKey);
    bool fFound = h.ht_Signature == (unsigned int)qwKey;

    if (!fFound) {
        qwEffectiveKey++;
        h = GetHTEntry(qwEffectiveKey);
        fFound = h.ht_Signature == (unsigned int)qwKey;
    }

    int nResult = Useless;

#if MP
    if (pLocalHT != NULL && !fFound) {
        h = pLocalHT[(qwKey >> 32) & L_HT_Mask];
        fFound = h.ht_Signature == (unsigned int)qwKey;
    }
#endif

    if (fFound) {
        *pBestm = h.ht_Move;
        *pThreat = (h.ht_Flags & HT_THREAT);

#if MP
        if ((int)h.ht_Depth == nDepth && nExclusiveP &&
            (h.ht_Flags & HT_NCPU) > 0) {

            nResult = OnEvaluation;

        } else
#endif

            if ((int)h.ht_Depth >= nDepth) {
            *pScore = h.ht_Score;

            /*
             * Correct a mate score. See comment in 'StoreHT'.
             */

            if (*pScore > CMLIMIT) {
                *pScore -= nPly;
            } else if (*pScore < -CMLIMIT) {
                *pScore += nPly;
            }

            if (h.ht_Flags & HT_EXACT) {
                nResult = ExactScore;
            } else if (h.ht_Flags & HT_LBOUND) {
                nResult = LowerBound;
            } else if (h.ht_Flags & HT_UBOUND) {
                nResult = UpperBound;
            }

#if MP
            if ((int)h.ht_Depth == nDepth) {

                /*
                 * increment processor count
                 */

                h.ht_Flags += HT_NCPU_INCREMENT;
                PutHTEntry(qwEffectiveKey, h);
            }
#endif /* MP */

        } else {
            nResult = Useful;
        }
    }
#if MP
    else {
        h.ht_Depth = (short)nDepth;
        h.ht_Flags = HT_NCPU_INCREMENT;
        h.ht_Signature = (int)qwKey;
        PutHTEntryBestEffort(qwKey, h, nDepth);
    }
#endif /* MP */

    return static_cast<LookupResult>(nResult);
}

LookupResult ProbePT(hash_t qwKey, int *pScore, struct PawnFacts *pf) {
#if MP
    acquire_read_lock(PawnMutex + ((qwKey >> 32) & MUTEX_MASK));
#endif /* MP */

    struct PTEntry h = PawnTable[(qwKey >> 32) & PT_Mask];

#if MP
    release_read_lock(PawnMutex + ((qwKey >> 32) & MUTEX_MASK));
#endif /* MP */

    if (h.pt_Signature == (unsigned int)qwKey && h.pt_Score != PT_INVALID) {
        *pScore = h.pt_Score;
        *pf = h.pt_PawnFacts;
        return Useful;
    }

    return Useless;
}

LookupResult ProbeST(hash_t qwKey, int *pScore) {
#if MP
    acquire_read_lock(ScoreMutex + ((qwKey >> 32) & MUTEX_MASK));
#endif /* MP */

    struct STEntry h = ScoreTable[(qwKey >> 32) & ST_Mask];

#if MP
    release_read_lock(ScoreMutex + ((qwKey >> 32) & MUTEX_MASK));
#endif /* MP */

    if (h.st_Signature == (unsigned int)qwKey && h.st_Score != PT_INVALID) {
        *pScore = h.st_Score;
        return Useful;
    }

    return Useless;
}

void StoreHT(hash_t qwKey, int best, int nAlpha, int beta, CMove bestm, int nDepth,
             int nThreat, int nPly
#if MP
             ,
             struct HTEntry *pLocalHT
#endif
) {
    hash_t qwEffectiveKey = qwKey;
    struct HTEntry Entry = GetHTEntry(qwEffectiveKey);
    bool fFound = Entry.ht_Signature == (unsigned int)qwKey;

    if (!fFound) {
        qwEffectiveKey++;
        Entry = GetHTEntry(qwEffectiveKey);
        fFound = Entry.ht_Signature == (unsigned int)qwKey;
    }

    HTStoreTried++;

#if MP
    if (!fFound) {
        Entry = pLocalHT[(qwKey >> 32) & L_HT_Mask];
    }
#endif

    int nReduced = best;

    /*
     * Handling of mate scores is a bit tricky.
     * Upon storing we correct it to mean 'mate in n from this position'
     * instead of 'mate in n from root position'. Upon retrieval this has
     * to be corrected.
     */

    if (best > CMLIMIT) {
        nReduced += nPly;
    } else if (best < -CMLIMIT) {
        nReduced -= nPly;
    }

#if MP
    if (Entry.ht_Signature == (unsigned int)qwKey && nDepth == Entry.ht_Depth) {
        if ((Entry.ht_Flags & HT_NCPU) > 0) {
            Entry.ht_Flags = (Entry.ht_Flags & HT_NCPU) - HT_NCPU_INCREMENT;
        }
    } else {
        Entry.ht_Signature = (unsigned int)qwKey;
        Entry.ht_Flags = 0;
    }
#else
    Entry.ht_Signature = (unsigned int)qwKey;
#endif /* MP */

    Entry.ht_Move = bestm;
    Entry.ht_Depth = (short)nDepth;
    Entry.ht_Score = nReduced;
#if MP
    Entry.ht_Flags |= (short)HTGeneration;
#else
    Entry.ht_Flags = (short)HTGeneration;
#endif /* MP */
    if (best <= nAlpha)
        Entry.ht_Flags |= HT_UBOUND;
    else if (best >= beta)
        Entry.ht_Flags |= HT_LBOUND;
    else
        Entry.ht_Flags |= HT_EXACT;

    if (nThreat)
        Entry.ht_Flags |= HT_THREAT;

    bool fSuccess = PutHTEntryBestEffort(qwKey, Entry, nDepth);
    if (!fSuccess) {
        HTStoreFailed++;
#if MP
        pLocalHT[(qwKey >> 32) & L_HT_Mask] = Entry;
#endif
    }
}

void StorePT(hash_t qwKey, int nScore, struct PawnFacts *pf) {
    struct PTEntry h = {(unsigned int)qwKey, nScore, *pf};

#if MP
    acquire_write_lock(PawnMutex + ((qwKey >> 32) & MUTEX_MASK));
#endif /* MP */

    PawnTable[(qwKey >> 32) & PT_Mask] = h;

#if MP
    release_write_lock(PawnMutex + ((qwKey >> 32) & MUTEX_MASK));
#endif /* MP */
}

void StoreST(hash_t qwKey, int nScore) {
    struct STEntry h = {(unsigned int)qwKey, nScore};

#if MP
    acquire_write_lock(ScoreMutex + ((qwKey >> 32) & MUTEX_MASK));
#endif /* MP */

    ScoreTable[(qwKey >> 32) & ST_Mask] = h;

#if MP
    release_write_lock(ScoreMutex + ((qwKey >> 32) & MUTEX_MASK));
#endif /* MP */
}

/* Moved this to a seperate routine to make the PB-Move
 * selection work better..
 */

void ClearHashTable(void) {
    unsigned int dwI;
    struct HTEntry *h = TranspositionTable;

    for (dwI = 0; dwI < HT_Size; dwI++, h++) {
        h->ht_Signature = 0;
        h->ht_Flags = 0;
    }
}

void AgeHashTable(void) {
    HTGeneration++;
    HTGeneration &= HT_AGE;

    HTStoreTried = 0;
    HTStoreFailed = 0;
}

void ClearPawnHashTable(void) {
    unsigned int dwI;
    struct PTEntry *ph;
    struct STEntry *pSh;

    ph = PawnTable;
    for (dwI = 0; dwI < PT_Size; dwI++, ph++) {
        ph->pt_Score = PT_INVALID;
    }

    pSh = ScoreTable;
    for (dwI = 0; dwI < ST_Size; dwI++, pSh++) {
        pSh->st_Score = PT_INVALID;
    }
}

static void FreeHT(void) {
    if (TranspositionTable) {
        free(TranspositionTable);
        TranspositionTable = NULL;
    }

    if (PawnTable) {
        free(PawnTable);
        PawnTable = NULL;
    }

    if (ScoreTable) {
        free(ScoreTable);
        ScoreTable = NULL;
    }
}

void AllocateHT(void) {
    static bool s_fRegisteredFreeHt = false;

    /*
     * Register atexit() handler to free hashtable memory automatically
     */

    if (!s_fRegisteredFreeHt) {
        s_fRegisteredFreeHt = true;
        atexit(FreeHT);
    }

    HT_Size = 1 << HT_Bits;
    HT_Mask = HT_Size - 1;

    TranspositionTable = (struct HTEntry *)safe_calloc(HT_Size, sizeof(struct HTEntry));

    /* Thread-local hash table - only calculate sizes and bits here...*/
    L_HT_Size = 1 << L_HT_Bits;
    L_HT_Mask = L_HT_Size - 1;

    PT_Size = 1 << PT_Bits;
    PT_Mask = PT_Size - 1;

    PawnTable = (struct PTEntry *)safe_calloc(PT_Size, sizeof(struct PTEntry));

    ST_Size = 1 << ST_Bits;
    ST_Mask = ST_Size - 1;

    ScoreTable = (struct STEntry *)safe_calloc(ST_Size, sizeof(struct STEntry));

    Print(0, "Hashtable sizes: %d k, %d k, %d k (%d, %d, %d bits)\n",
          (int)((((int64_t)1 << HT_Bits) * (int64_t)sizeof(struct HTEntry)) /
                1024),
          (int)((((int64_t)1 << PT_Bits) * (int64_t)sizeof(struct PTEntry)) /
                1024),
          (int)((((int64_t)1 << ST_Bits) * (int64_t)sizeof(struct STEntry)) /
                1024),
          HT_Bits, PT_Bits, ST_Bits);

#if MP
    for (int nI = 0; nI < MUTEX_COUNT; nI++) {
        TranspositionMutex[nI] = 0;
        PawnMutex[nI] = 0;
        ScoreMutex[nI] = 0;
    }
#endif
}

void ShowHashStatistics(void) {
    unsigned int dwI;
    unsigned int dwCnt = 0;
    struct HTEntry *h = TranspositionTable;

    for (dwI = 0; dwI < HT_Size; dwI++, h++) {
        if ((h->ht_Flags & HT_AGE) == HTGeneration)
            dwCnt++;
    }

    char szBuf1[16], szBuf2[16];

    Print(1, "Hashtable 1:  entries = %s, use = %s (%d %%)\n",
          FormatCount(dwI, szBuf1, sizeof(szBuf1)),
          FormatCount(dwCnt, szBuf2, sizeof(szBuf2)), Percentage(dwCnt, dwI));
    Print(1, "              store failed = %s (%d %%)\n",
          FormatCount(HTStoreFailed, szBuf1, sizeof(szBuf1)),
          Percentage(HTStoreFailed, HTStoreTried));
}

void GuessHTSizes(char *pszSize) {
    size_t qwLast = strlen(pszSize) - 1;
    int64_t nTotalSize;
    int64_t nTmp;

    if (pszSize[qwLast] == 'k') {
        nTotalSize = atoi(pszSize) * 1024L;
    } else if (pszSize[qwLast] == 'm') {
        nTotalSize = atoi(pszSize) * 1024L * 1024L;
    } else {
        nTotalSize = atoi(pszSize) * 1024;
    }

    if (nTotalSize < 64 * 1024) {
        Print(0, "I need at least 64k of hashtables.\n");
        nTotalSize = 64 * 1024;
    }

    nTmp = nTotalSize * 4 / 5;

    for (HT_Bits = 1; HT_Bits < 32; HT_Bits++) {
        int64_t nTmp2 =
            ((int64_t)1 << (HT_Bits + 1)) * (int64_t)sizeof(struct HTEntry);
        if (nTmp2 > nTmp)
            break;
    }

    nTotalSize -= ((int64_t)1 << HT_Bits) * (int64_t)sizeof(struct HTEntry);

    nTmp = 3 * nTotalSize / 4;

    for (ST_Bits = 1; ST_Bits < 32; ST_Bits++) {
        int64_t nTmp2 =
            ((int64_t)1 << (ST_Bits + 1)) * (int64_t)sizeof(struct STEntry);
        if (nTmp2 > nTmp)
            break;
    }

    nTotalSize -= ((int64_t)1 << ST_Bits) * (int64_t)sizeof(struct STEntry);

    for (PT_Bits = 1; PT_Bits < 32; PT_Bits++) {
        int64_t nTmp2 =
            ((int64_t)1 << (PT_Bits + 1)) * (int64_t)sizeof(struct PTEntry);
        if (nTmp2 > nTotalSize)
            break;
    }
}

void HashInit(void) {
    unsigned int dwI, dwJ, dwK;

    InitRandom(0);

    for (dwI = 0; dwI < 2; dwI++) {
        for (dwJ = 0; dwJ < 8; dwJ++) {
            for (dwK = 0; dwK < CBitBoard::SIZE; dwK++) {
                HashKeys[dwI][dwJ][dwK] = Random64();
            }
        }
    }

    for (dwI = 0; dwI < CBitBoard::SIZE; dwI++) {
        HashKeysEP[dwI] = Random64();
    }

    for (dwI = 0; dwI < 16; dwI++) {
        HashKeysCastle[dwI] = Random64();
    }

    STMKey = Random64();
}
