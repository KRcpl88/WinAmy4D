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
#include "init.h"
#include "inline.h"
#include "scoord.h"
#include "utils.h"

CBitBoard ShiftUpMask, ShiftDownMask;
CBitBoard ShiftLeftMask, ShiftRightMask;

CBitBoard FileMask[8], IsoMask[8];
CBitBoard RankMask[8];
CBitBoard ForwardRayW[CBitBoard::SIZE], ForwardRayB[CBitBoard::SIZE];
CBitBoard PassedMaskW[CBitBoard::SIZE], PassedMaskB[CBitBoard::SIZE];
CBitBoard OutpostMaskW[CBitBoard::SIZE], OutpostMaskB[CBitBoard::SIZE];
CBitBoard InterPath[CBitBoard::SIZE][CBitBoard::SIZE];
CBitBoard Ray[CBitBoard::SIZE][CBitBoard::SIZE];
CBitBoard WPawnEPM[CBitBoard::SIZE], BPawnEPM[CBitBoard::SIZE];
CBitBoard BishopEPM[CBitBoard::SIZE], RookEPM[CBitBoard::SIZE], QueenEPM[CBitBoard::SIZE];
CBitBoard SeventhRank[2], EighthRank[2];
CBitBoard ThirdRank[2];
CBitBoard LeftOf[8], RightOf[8], FarLeftOf[8], FarRightOf[8];
CBitBoard EdgeMask;
CBitBoard BlackSquaresMask, WhiteSquaresMask;
CBitBoard KingSquareW[CBitBoard::SIZE], KingSquareB[CBitBoard::SIZE];
CBitBoard NotAFileMask, NotHFileMask;
CBitBoard CornerMaskA1, CornerMaskA8, CornerMaskH1, CornerMaskH8;
CBitBoard WPawnBackwardMask[CBitBoard::SIZE], BPawnBackwardMask[CBitBoard::SIZE];
CBitBoard KingSideMask, QueenSideMask;
CBitBoard ConnectedMask[CBitBoard::SIZE];

void InitMasks(void) {
    const CBitBoard allOnes = ~CBitBoard();
    ShiftUpMask = ShiftDownMask = ShiftLeftMask = ShiftRightMask = allOnes;
    for (unsigned int i = 0; i < CBitBoard::MAX_LEVEL_WIDTH; i++) {
        ShiftUpMask &= CBitBoard::ClrMask(static_cast<uint16_t>(i));
        ShiftDownMask &= CBitBoard::ClrMask(
            static_cast<uint16_t>(CBitBoard::SIZE - CBitBoard::MAX_LEVEL_WIDTH + i));
        ShiftRightMask &= CBitBoard::ClrMask(static_cast<uint16_t>(
            CBitBoard::MAX_LEVEL_WIDTH * i + (CBitBoard::MAX_LEVEL_WIDTH - 1)));
        ShiftLeftMask &= CBitBoard::ClrMask(
            static_cast<uint16_t>(CBitBoard::MAX_LEVEL_WIDTH * i));
    }
}

void PrintBitBoard(CBitBoard x) {
    for (unsigned int level = 0; level < CBitBoard::NUM_LEVELS; level++) {
        const unsigned int width = CBitBoard::LEVEL_WIDTH[level];
        for (int rank = static_cast<int>(width) - 1; rank >= 0; rank--) {
            for (unsigned int file = 0; file < width; file++) {
                int k =
                    static_cast<int>(CSCoord(static_cast<int>(level), static_cast<int>(file), rank));
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
    const int maxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int size = static_cast<int>(CBitBoard::SIZE);

    for (i = 0; i < maxLevelWidth; i++) {
        FileMask[i] = {};
        IsoMask[i] = {};
    }

    for (unsigned int level = 0; level < CBitBoard::NUM_LEVELS; level++) {
        const unsigned int width = CBitBoard::LEVEL_WIDTH[level];
        for (unsigned int file = 0; file < width; file++) {
            for (unsigned int rank = 0; rank < width; rank++) {
                const uint16_t square =
                    CSCoord(static_cast<uint16_t>(level), static_cast<uint16_t>(file),
                            static_cast<uint16_t>(rank))
                        .BitOffset();
                FileMask[file] |= CBitBoard::SetMask(square);
                if (file > 0) {
                    IsoMask[file] |= CBitBoard::SetMask(
                        CSCoord(static_cast<uint16_t>(level),
                                static_cast<uint16_t>(file - 1),
                                static_cast<uint16_t>(rank))
                            .BitOffset());
                }
                if ((file + 1) < width) {
                    IsoMask[file] |= CBitBoard::SetMask(
                        CSCoord(static_cast<uint16_t>(level),
                                static_cast<uint16_t>(file + 1),
                                static_cast<uint16_t>(rank))
                            .BitOffset());
                }
            }
#ifdef DEBUG
            PrintBitBoard(IsoMask[file]);
#endif
        }
    }
    for (i = 0; i < size; i++) {
        ForwardRayW[i] = ForwardRayB[i] = {};
        for (j = i + 8; j < size; j += 8) {
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
    for (i = 0; i < size; i++) {
        const CSCoord coord(i);
        const uint16_t width = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[coord.m_nLevel]);
        PassedMaskW[i] = ForwardRayW[i];
        if (coord.m_nFile > 0)
            PassedMaskW[i] |= ForwardRayW[i - 1];
        if (coord.m_nFile < (width - 1))
            PassedMaskW[i] |= ForwardRayW[i + 1];
        PassedMaskB[i] = ForwardRayB[i];
        if (coord.m_nFile > 0)
            PassedMaskB[i] |= ForwardRayB[i - 1];
        if (coord.m_nFile < (width - 1))
            PassedMaskB[i] |= ForwardRayB[i + 1];
        /* PrintBitBoard(PassedMaskW[i]); */
        /* PrintBitBoard(PassedMaskB[i]); */
        OutpostMaskW[i] = OutpostMaskB[i] = {};
        if (coord.m_nFile > 0) {
            OutpostMaskW[i] |= ForwardRayW[i - 1];
            OutpostMaskB[i] |= ForwardRayB[i - 1];
        }
        if (coord.m_nFile < (width - 1)) {
            OutpostMaskW[i] |= ForwardRayW[i + 1];
            OutpostMaskB[i] |= ForwardRayB[i + 1];
        }
        /*
        printf("\n%c%c:\n", SQUARE(i));
        Print2BitBoards(ArtIsoMaskW[i], ArtIsoMaskB[i]);
        */
    }

    for (i = 0; i < size; i++) {
        int sq;

        WPawnBackwardMask[i] = BPawnBackwardMask[i] = {};
        for (sq = i; sq > 0; sq -= 8) {
            const CSCoord sqCoord(sq);
            const unsigned int sqFile = static_cast<unsigned int>(sqCoord.m_nFile);
            const unsigned int levelWidth =
                CBitBoard::LEVEL_WIDTH[static_cast<unsigned int>(sqCoord.m_nLevel)];
            if (sqCoord.m_nFile > 0) {
                WPawnBackwardMask[i] |=
                    CBitBoard::SetMask(static_cast<uint16_t>(sq - 1));
            }
            if (sqFile < (levelWidth - 1)) {
                WPawnBackwardMask[i] |=
                    CBitBoard::SetMask(static_cast<uint16_t>(sq + 1));
            }
        }
        for (sq = i; sq < size; sq += 8) {
            const CSCoord sqCoord(sq);
            const unsigned int sqFile = static_cast<unsigned int>(sqCoord.m_nFile);
            const unsigned int levelWidth =
                CBitBoard::LEVEL_WIDTH[static_cast<unsigned int>(sqCoord.m_nLevel)];
            if (sqCoord.m_nFile > 0) {
                BPawnBackwardMask[i] |=
                    CBitBoard::SetMask(static_cast<uint16_t>(sq - 1));
            }
            if (sqFile < (levelWidth - 1)) {
                BPawnBackwardMask[i] |=
                    CBitBoard::SetMask(static_cast<uint16_t>(sq + 1));
            }
        }
    }

    for (i = 0; i < size; i++) {
        const CSCoord iCoord(i);
        const uint16_t width = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[iCoord.m_nLevel]);
        ConnectedMask[i] = {};

        if (iCoord.m_nFile < (width - 1)) {
            ConnectedMask[i].SetBit(i + 1);
            if (iCoord.m_nRank > 1) {
                ConnectedMask[i].SetBit(i - 7);
            }
            if (iCoord.m_nRank < (width - 2)) {
                ConnectedMask[i].SetBit(i + 9);
            }
        }
        if (iCoord.m_nFile > 0) {
            ConnectedMask[i].SetBit(i - 1);
            if (iCoord.m_nRank > 1) {
                ConnectedMask[i].SetBit(i - 9);
            }
            if (iCoord.m_nRank < (width - 2)) {
                ConnectedMask[i].SetBit(i + 7);
            }
        }
    }
}

void InitGeometry(void) {
    int edge[100];
    int trto[100];
    int i, j, k, l;
    const int maxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int size = static_cast<int>(CBitBoard::SIZE);
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
            if (x >= 0 && y >= 0 && x < maxLevelWidth && y < maxLevelWidth) {
                trto[i + 10 * j] = x + maxLevelWidth * y;
            }
        }
    }

    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            InterPath[i][j] = {};
            Ray[i][j] = {};
        }
        WPawnEPM[i] = BPawnEPM[i] = BishopEPM[i] = RookEPM[i] = QueenEPM[i] =
            {};
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
    const int maxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int size = static_cast<int>(CBitBoard::SIZE);

    SeventhRank[White] = SeventhRank[Black] = {};
    EighthRank[White] = EighthRank[Black] = {};
    ThirdRank[White] = ThirdRank[Black] = {};

    for (i = 0; i < maxLevelWidth; i++) {
        RankMask[i] = {};
    }

    for (unsigned int level = 0; level < CBitBoard::NUM_LEVELS; level++) {
        const unsigned int width = CBitBoard::LEVEL_WIDTH[level];
        for (unsigned int rank = 0; rank < width; rank++) {
            for (unsigned int file = 0; file < width; file++) {
                const int square = static_cast<int>(
                    CSCoord(static_cast<int>(level), static_cast<int>(file), static_cast<int>(rank)));
                RankMask[rank] |= CBitBoard::SetMask(square);

                if (rank == 6) {
                    SeventhRank[White] |= CBitBoard::SetMask(square);
                }
                if (rank == 1) {
                    SeventhRank[Black] |= CBitBoard::SetMask(square);
                }
                if (rank == 7) {
                    EighthRank[White] |= CBitBoard::SetMask(square);
                }
                if (rank == 0) {
                    EighthRank[Black] |= CBitBoard::SetMask(square);
                }
                if (rank == 2) {
                    ThirdRank[White] |= CBitBoard::SetMask(square);
                }
                if (rank == 5) {
                    ThirdRank[Black] |= CBitBoard::SetMask(square);
                }
            }
        }
    }

    for (i = 0; i < maxLevelWidth; i++) {
        LeftOf[i] = RightOf[i] = FarLeftOf[i] = FarRightOf[i] = {};
        for (j = i - 1; j >= 0; j--)
            LeftOf[i] |= FileMask[j];
        for (j = i - 2; j >= 0; j--)
            FarLeftOf[i] |= FileMask[j];
        for (j = i + 1; j < maxLevelWidth; j++)
            RightOf[i] |= FileMask[j];
        for (j = i + 2; j < maxLevelWidth; j++)
            FarRightOf[i] |= FileMask[j];
    }

    EdgeMask = {};

    for (i = 0; i < maxLevelWidth; i++) {
        EdgeMask.SetBit(ha1 + i);
        EdgeMask.SetBit(ha8 + i);
        EdgeMask.SetBit(ha1 + maxLevelWidth * i);
        EdgeMask.SetBit(hh1 + maxLevelWidth * i);
    }

    WhiteSquaresMask = BlackSquaresMask = {};
    for (unsigned int level = 0; level < CBitBoard::NUM_LEVELS; level++) {
        const unsigned int width = CBitBoard::LEVEL_WIDTH[level];
        for (unsigned int rank = 0; rank < width; rank++) {
            for (unsigned int file = 0; file < width; file++) {
                const int square = static_cast<int>(
                    CSCoord(static_cast<int>(level), static_cast<int>(file), static_cast<int>(rank)));
                if (((rank + file) & 1) == 0) {
                    BlackSquaresMask.SetBit(square);
                } else {
                    WhiteSquaresMask.SetBit(square);
                }
            }
        }
    }

    for (i = 0; i < size; i++) {
        const CSCoord coord(i);
        const uint16_t width = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[coord.m_nLevel]);
        int bdist = coord.m_nRank;
        int wdist = (width - 1) - bdist;
        CSCoord wtargetCoord(coord.m_nLevel, coord.m_nFile, width - 1);
        CSCoord btargetCoord(coord.m_nLevel, coord.m_nFile, 0);

        KingSquareW[i] = KingSquareB[i] = {};
        for (j = 0; j < size; j++) {
            CSCoord coord(j);
            if (KingDist(wtargetCoord, coord) <= wdist) {
                KingSquareW[i].SetBit(j);
            }
            if (KingDist(btargetCoord, coord) <= bdist) {
                KingSquareB[i].SetBit(j);
            }
        }
    }

    NotAFileMask = NotHFileMask = {};
    for (i = 0; i < 7; i++) {
        NotAFileMask |= FileMask[i + 1];
        NotHFileMask |= FileMask[i];
    }

    CornerMaskA1 = CBitBoard::SetMask(ha1) | CBitBoard::SetMask(ha2) | CBitBoard::SetMask(hb1) | CBitBoard::SetMask(hb2);
    CornerMaskA8 = CBitBoard::SetMask(ha8) | CBitBoard::SetMask(ha7) | CBitBoard::SetMask(hb8) | CBitBoard::SetMask(hb7);
    CornerMaskH1 = CBitBoard::SetMask(hh1) | CBitBoard::SetMask(hh2) | CBitBoard::SetMask(hg1) | CBitBoard::SetMask(hg2);
    CornerMaskH8 = CBitBoard::SetMask(hh8) | CBitBoard::SetMask(hh7) | CBitBoard::SetMask(hg8) | CBitBoard::SetMask(hg7);

    KingSideMask = FileMask[7] | FileMask[6] | FileMask[5] | FileMask[4];
    QueenSideMask = FileMask[0] | FileMask[1] | FileMask[2] | FileMask[3];
}

void InitAll(void) {
    InitMasks();
    InitPawnMasks();
    InitGeometry();
    InitGeometry3D();
    InitMiscMasks();
    InitNextSQ();
}
