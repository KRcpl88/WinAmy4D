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
 * init.c - initialization routines
 */

#include "dbase.h"
#include "inline.h"
#include "magic.h"
#include "scoord.h"
#include "utils.h"

CBitBoard ShiftUpMask, ShiftDownMask;
CBitBoard ShiftLeftMask, ShiftRightMask;

CBitBoard FileMask[8], IsoMask[8];
CBitBoard RankMask[8];
CBitBoard ForwardRayW[64], ForwardRayB[64];
CBitBoard PassedMaskW[64], PassedMaskB[64];
CBitBoard OutpostMaskW[64], OutpostMaskB[64];
CBitBoard InterPath[64][64];
CBitBoard Ray[64][64];
CBitBoard WPawnEPM[64], BPawnEPM[64];
CBitBoard BishopEPM[64], RookEPM[64], QueenEPM[64];
CBitBoard SeventhRank[2], EighthRank[2];
CBitBoard ThirdRank[2];
CBitBoard LeftOf[8], RightOf[8], FarLeftOf[8], FarRightOf[8];
CBitBoard EdgeMask;
CBitBoard BlackSquaresMask, WhiteSquaresMask;
CBitBoard KingSquareW[64], KingSquareB[64];
CBitBoard NotAFileMask, NotHFileMask;
CBitBoard CornerMaskA1, CornerMaskA8, CornerMaskH1, CornerMaskH8;
CBitBoard WPawnBackwardMask[64], BPawnBackwardMask[64];
CBitBoard KingSideMask, QueenSideMask;
CBitBoard ConnectedMask[64];

void InitMasks(void) {
    int i;

    ShiftUpMask = ShiftDownMask = ShiftLeftMask = ShiftRightMask = -1;
    for (i = 0; i < 8; i++) {
        ShiftUpMask &= CBitBoard::ClrMask(i);
        ShiftDownMask &= CBitBoard::ClrMask(56 + i);
        ShiftRightMask &= CBitBoard::ClrMask(8 * i + 7);
        ShiftLeftMask &= CBitBoard::ClrMask(8 * i);
    }
}

void PrintBitBoard(CBitBoard x) {
    for (int level = 0; level < CSCoord::NUM_LEVELS; level++) {
        const int width = CSCoord::LEVEL_WIDTH[level];
        for (int rank = width - 1; rank >= 0; rank--) {
            for (int file = 0; file < width; file++) {
                int k = static_cast<int>(CSCoord(level, file, rank));
                if (x.TstBit(k))
                    Print(0, "*");
                else
                    Print(0, ".");
            }
            Print(0, "\n");
        }
    }
}

void InitPawnMasks(void) {
    int i, j;

    for (i = 0; i < CSCoord::MAX_LEVEL_WIDTH; i++) {
        FileMask[i] = 0;
        IsoMask[i] = 0;
    }

    for (int level = 0; level < CSCoord::NUM_LEVELS; level++) {
        const int width = CSCoord::LEVEL_WIDTH[level];
        for (i = 0; i < width; i++) {
            for (j = 0; j < width; j++) {
                const int square = static_cast<int>(CSCoord(level, i, j));
                FileMask[i] |= CBitBoard::SetMask(square);
                if (i > 0)
                    IsoMask[i] |= CBitBoard::SetMask(static_cast<int>(CSCoord(level, i - 1, j)));
                if (i + 1 < width)
                    IsoMask[i] |= CBitBoard::SetMask(static_cast<int>(CSCoord(level, i + 1, j)));
            }
#ifdef DEBUG
            PrintBitBoard(IsoMask[i]);
#endif
        }
    }
    for (i = 0; i < 64; i++) {
        ForwardRayW[i] = ForwardRayB[i] = 0;
        for (j = i + 8; j < 64; j += 8) {
            ForwardRayW[i] |= CBitBoard::SetMask(j);
        }
        for (j = i - 8; j >= 0; j -= 8) {
            ForwardRayB[i] |= CBitBoard::SetMask(j);
        }
#ifdef DEBUG
        PrintBitBoard(ForwardRayW[i]);
        PrintBitBoard(ForwardRayB[i]);
#endif
    }
    for (i = 0; i < 64; i++) {
        PassedMaskW[i] = ForwardRayW[i];
        if ((i & 7) > 0)
            PassedMaskW[i] |= ForwardRayW[i - 1];
        if ((i & 7) < 7)
            PassedMaskW[i] |= ForwardRayW[i + 1];
        PassedMaskB[i] = ForwardRayB[i];
        if ((i & 7) > 0)
            PassedMaskB[i] |= ForwardRayB[i - 1];
        if ((i & 7) < 7)
            PassedMaskB[i] |= ForwardRayB[i + 1];
        /* PrintBitBoard(PassedMaskW[i]); */
        /* PrintBitBoard(PassedMaskB[i]); */
        OutpostMaskW[i] = OutpostMaskB[i] = 0;
        if ((i & 7) > 0) {
            OutpostMaskW[i] |= ForwardRayW[i - 1];
            OutpostMaskB[i] |= ForwardRayB[i - 1];
        }
        if ((i & 7) < 7) {
            OutpostMaskW[i] |= ForwardRayW[i + 1];
            OutpostMaskB[i] |= ForwardRayB[i + 1];
        }
        /*
        printf("\n%c%c:\n", SQUARE(i));
        Print2BitBoards(ArtIsoMaskW[i], ArtIsoMaskB[i]);
        */
    }

    for (i = 0; i < 64; i++) {
        int sq;

        WPawnBackwardMask[i] = BPawnBackwardMask[i] = 0;
        for (sq = i; sq > 0; sq -= 8) {
            if ((sq & 7) > 0) {
                WPawnBackwardMask[i] |= CBitBoard::SetMask(sq - 1);
            }
            if ((sq & 7) < 7) {
                WPawnBackwardMask[i] |= CBitBoard::SetMask(sq + 1);
            }
        }
        for (sq = i; sq < 64; sq += 8) {
            if ((sq & 7) > 0) {
                BPawnBackwardMask[i] |= CBitBoard::SetMask(sq - 1);
            }
            if ((sq & 7) < 7) {
                BPawnBackwardMask[i] |= CBitBoard::SetMask(sq + 1);
            }
        }
    }

    for (i = 0; i < 64; i++) {
        ConnectedMask[i] = 0;

        if ((i & 7) < 7) {
            ConnectedMask[i].SetBit(i + 1);
            if ((i >> 3) > 1) {
                ConnectedMask[i].SetBit(i - 7);
            }
            if ((i >> 3) < 6) {
                ConnectedMask[i].SetBit(i + 9);
            }
        }
        if ((i & 7) > 0) {
            ConnectedMask[i].SetBit(i - 1);
            if ((i >> 3) > 1) {
                ConnectedMask[i].SetBit(i - 9);
            }
            if ((i >> 3) < 6) {
                ConnectedMask[i].SetBit(i + 7);
            }
        }
    }
}

void InitGeometry(void) {
    int edge[100];
    int trto[100];
    int i, j, k, l;
    int dirs[] = {1, -1, 10, -10, 9, -9, 11, -11};
    int dirb[] = {9, -9, 11, -11};
    int dirr[] = {1, -1, 10, -10};

    for (i = 0; i < 100; i++) {
        edge[i] = 0;
        trto[i] = 0;
    }

    for (i = 0; i < 10; i++) {
        edge[i] = edge[90 + i] = edge[10 * i] = edge[10 * i + 9] = 1;
        for (j = 0; j < 10; j++) {
            int x = i - 1;
            int y = j - 1;
            if (x >= 0 && y >= 0 && x < 8 && y < 8) {
                trto[i + 10 * j] = x + 8 * y;
            }
        }
    }

    for (i = 0; i < 64; i++) {
        for (j = 0; j < 64; j++) {
            InterPath[i][j] = 0;
            Ray[i][j] = 0;
        }
        WPawnEPM[i] = BPawnEPM[i] = BishopEPM[i] = RookEPM[i] = QueenEPM[i] =
            0ULL;
    }

    for (j = 0; j < 100; j++) {
        int x = trto[j];
        if (edge[j])
            continue;
        for (i = 0; i < 8; i++) {
            int d = dirs[i];
            for (k = j + d; !edge[k]; k += d) {
                int y = trto[k];
                for (l = j + d; l != k; l += d)
                    InterPath[x][y] |= CBitBoard::SetMask(trto[l]);
                for (l = k + d; !edge[l]; l += d)
                    Ray[x][y] |= CBitBoard::SetMask(trto[l]);
            }
        }
        for (i = 0; i < 4; i++) {
            int d = dirb[i];
            for (k = j + d; !edge[k]; k += d) {
                BishopEPM[x] |= CBitBoard::SetMask(trto[k]);
                QueenEPM[x] |= CBitBoard::SetMask(trto[k]);
            }
            d = dirr[i];
            for (k = j + d; !edge[k]; k += d) {
                RookEPM[x] |= CBitBoard::SetMask(trto[k]);
                QueenEPM[x] |= CBitBoard::SetMask(trto[k]);
            }
        }
        if (!edge[j + 9])
            WPawnEPM[x] |= CBitBoard::SetMask(x + 7);
        if (!edge[j + 11])
            WPawnEPM[x] |= CBitBoard::SetMask(x + 9);
        if (!edge[j - 9])
            BPawnEPM[x] |= CBitBoard::SetMask(x - 7);
        if (!edge[j - 11])
            BPawnEPM[x] |= CBitBoard::SetMask(x - 9);
    }
}

void InitMiscMasks(void) {
    int i, j;

    SeventhRank[White] = SeventhRank[Black] = 0;
    EighthRank[White] = EighthRank[Black] = 0;
    ThirdRank[White] = ThirdRank[Black] = 0;

    for (i = 0; i < CSCoord::MAX_LEVEL_WIDTH; i++) {
        RankMask[i] = 0;
    }

    for (int level = 0; level < CSCoord::NUM_LEVELS; level++) {
        const int width = CSCoord::LEVEL_WIDTH[level];
        for (i = 0; i < width; i++) {
            for (j = 0; j < width; j++) {
                const int square = static_cast<int>(CSCoord(level, j, i));
                RankMask[i] |= CBitBoard::SetMask(square);

                if (i == 6) {
                    SeventhRank[White] |= CBitBoard::SetMask(square);
                }
                if (i == 1) {
                    SeventhRank[Black] |= CBitBoard::SetMask(square);
                }
                if (i == 7) {
                    EighthRank[White] |= CBitBoard::SetMask(square);
                }
                if (i == 0) {
                    EighthRank[Black] |= CBitBoard::SetMask(square);
                }
                if (i == 2) {
                    ThirdRank[White] |= CBitBoard::SetMask(square);
                }
                if (i == 5) {
                    ThirdRank[Black] |= CBitBoard::SetMask(square);
                }
            }
        }
    }

    for (i = 0; i < 8; i++) {
        LeftOf[i] = RightOf[i] = FarLeftOf[i] = FarRightOf[i] = 0;
        for (j = i - 1; j >= 0; j--)
            LeftOf[i] |= FileMask[j];
        for (j = i - 2; j >= 0; j--)
            FarLeftOf[i] |= FileMask[j];
        for (j = i + 1; j < 8; j++)
            RightOf[i] |= FileMask[j];
        for (j = i + 2; j < 8; j++)
            FarRightOf[i] |= FileMask[j];
    }

    EdgeMask = 0;

    for (i = 0; i < 8; i++) {
        EdgeMask.SetBit(a1 + i);
        EdgeMask.SetBit(a8 + i);
        EdgeMask.SetBit(a1 + 8 * i);
        EdgeMask.SetBit(h1 + 8 * i);
    }

    WhiteSquaresMask = BlackSquaresMask = 0;
    for (int level = 0; level < CSCoord::NUM_LEVELS; level++) {
        const int width = CSCoord::LEVEL_WIDTH[level];
        for (i = 0; i < width; i++) {
            for (j = 0; j < width; j++) {
                const int square = static_cast<int>(CSCoord(level, j, i));
                if (((i + j) & 1) == 0) {
                    BlackSquaresMask.SetBit(square);
                } else {
                    WhiteSquaresMask.SetBit(square);
                }
            }
        }
    }

    for (i = 0; i < 64; i++) {
        int bdist = (i >> 3);
        int wdist = 7 - bdist;
        int wtarget = (i & 7) + a8;
        int btarget = (i & 7) + a1;

        KingSquareW[i] = KingSquareB[i] = 0;
        for (j = 0; j < 64; j++) {
            if (KingDist(wtarget, j) <= wdist) {
                KingSquareW[i].SetBit(j);
            }
            if (KingDist(btarget, j) <= bdist) {
                KingSquareB[i].SetBit(j);
            }
        }
    }

    NotAFileMask = NotHFileMask = 0;
    for (i = 0; i < 7; i++) {
        NotAFileMask |= FileMask[i + 1];
        NotHFileMask |= FileMask[i];
    }

    CornerMaskA1 = CBitBoard::SetMask(a1) | CBitBoard::SetMask(a2) | CBitBoard::SetMask(b1) | CBitBoard::SetMask(b2);
    CornerMaskA8 = CBitBoard::SetMask(a8) | CBitBoard::SetMask(a7) | CBitBoard::SetMask(b8) | CBitBoard::SetMask(b7);
    CornerMaskH1 = CBitBoard::SetMask(h1) | CBitBoard::SetMask(h2) | CBitBoard::SetMask(g1) | CBitBoard::SetMask(g2);
    CornerMaskH8 = CBitBoard::SetMask(h8) | CBitBoard::SetMask(h7) | CBitBoard::SetMask(g8) | CBitBoard::SetMask(g7);

    KingSideMask = FileMask[7] | FileMask[6] | FileMask[5] | FileMask[4];
    QueenSideMask = FileMask[0] | FileMask[1] | FileMask[2] | FileMask[3];
}

void InitAll(void) {
    InitMasks();
    InitPawnMasks();
    InitGeometry();
    InitMiscMasks();
    InitMagic();
}
