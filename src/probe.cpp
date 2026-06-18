/*
 * probe.c - EGTB probing code
 *
 * This file is part of Amy, a chess program by Thorsten Greiner
 *
 * Amy is copyrighted by Thorsten Greiner
 *
 */

/*
 * The original endgame tablebase support relied on tbindex.cpp, which computes
 * tablebase indices using a fixed 8x8 (64-square) 2D chess board. That indexing
 * scheme is invalid for the 4D variant (square offsets range 0-343), so the
 * tablebase code has been removed. EGTB probing is therefore disabled and the
 * engine relies on regular search for these positions.
 */

#include "amy.h"

#include "probe.h"

int EGTBProbe, EGTBProbeSucc;

void InitEGTB(char *tbpath) { (void)tbpath; }

int ProbeEGTB(const CPosition *p, int *score, int ply) {
    (void)p;
    (void)score;
    (void)ply;
    return 0;
}
