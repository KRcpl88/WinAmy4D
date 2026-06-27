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
CBitBoard PrePromoRank[2];
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
    const CBitBoard AllOnes = ~CBitBoard();
    ShiftUpMask = ShiftDownMask = ShiftLeftMask = ShiftRightMask = AllOnes;
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

void PrintBitBoard(CBitBoard X) {
    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int dwWidth = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (int nRank = static_cast<int>(dwWidth) - 1; nRank >= 0; nRank--) {
            for (unsigned int dwFile = 0; dwFile < dwWidth; dwFile++) {
                int nK =
                    static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFile), nRank));
                if (X.TstBit(nK))
                    Print(0, "*");
                else
                    Print(0, ".");
            }
            Print(0, "\n");
        }
    }
}

void InitPawnMasks(void) {
    int i;
    const int nMaxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int nSize = static_cast<int>(CBitBoard::SIZE);

    for (i = 0; i < nMaxLevelWidth; i++) {
        FileMask[i] = {};
        IsoMask[i] = {};
    }

    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int dwWidth = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (unsigned int dwFile = 0; dwFile < dwWidth; dwFile++) {
            for (unsigned int dwRank = 0; dwRank < dwWidth; dwRank++) {
                const uint16_t wSquare =
                    CSCoord(static_cast<uint16_t>(dwLevel), static_cast<uint16_t>(dwFile),
                            static_cast<uint16_t>(dwRank))
                        .BitOffset();
                FileMask[dwFile] |= CBitBoard::SetMask(wSquare);
                if (dwFile > 0) {
                    IsoMask[dwFile] |= CBitBoard::SetMask(
                        CSCoord(static_cast<uint16_t>(dwLevel),
                                static_cast<uint16_t>(dwFile - 1),
                                static_cast<uint16_t>(dwRank))
                            .BitOffset());
                }
                if ((dwFile + 1) < dwWidth) {
                    IsoMask[dwFile] |= CBitBoard::SetMask(
                        CSCoord(static_cast<uint16_t>(dwLevel),
                                static_cast<uint16_t>(dwFile + 1),
                                static_cast<uint16_t>(dwRank))
                            .BitOffset());
                }
            }
#ifdef DEBUG
            PrintBitBoard(IsoMask[dwFile]);
#endif
        }
    }
    for (i = 0; i < nSize; i++) {
        ForwardRayW[i] = ForwardRayB[i] = {};
        const CSCoord Coord(static_cast<uint16_t>(i));
        const uint16_t wWidth = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[Coord.m_nLevel]);
        for (uint16_t r = Coord.m_nRank + 1; r < wWidth; r++) {
            ForwardRayW[i].SetBit(
                CSCoord(Coord.m_nLevel, Coord.m_nFile, r).BitOffset());
        }
        for (int r = static_cast<int>(Coord.m_nRank) - 1; r >= 0; r--) {
            ForwardRayB[i].SetBit(
                CSCoord(Coord.m_nLevel, Coord.m_nFile, static_cast<uint16_t>(r))
                    .BitOffset());
        }
#ifdef DEBUG
        PrintBitBoard(ForwardRayW[i]);
        PrintBitBoard(ForwardRayB[i]);
#endif
    }
    for (i = 0; i < nSize; i++) {
        const CSCoord Coord(i);
        const uint16_t wWidth = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[Coord.m_nLevel]);
        PassedMaskW[i] = ForwardRayW[i];
        if (Coord.m_nFile > 0)
            PassedMaskW[i] |= ForwardRayW[i - 1];
        if (Coord.m_nFile < (wWidth - 1))
            PassedMaskW[i] |= ForwardRayW[i + 1];
        PassedMaskB[i] = ForwardRayB[i];
        if (Coord.m_nFile > 0)
            PassedMaskB[i] |= ForwardRayB[i - 1];
        if (Coord.m_nFile < (wWidth - 1))
            PassedMaskB[i] |= ForwardRayB[i + 1];
        /* PrintBitBoard(PassedMaskW[i]); */
        /* PrintBitBoard(PassedMaskB[i]); */
        OutpostMaskW[i] = OutpostMaskB[i] = {};
        if (Coord.m_nFile > 0) {
            OutpostMaskW[i] |= ForwardRayW[i - 1];
            OutpostMaskB[i] |= ForwardRayB[i - 1];
        }
        if (Coord.m_nFile < (wWidth - 1)) {
            OutpostMaskW[i] |= ForwardRayW[i + 1];
            OutpostMaskB[i] |= ForwardRayB[i + 1];
        }
        /*
        printf("\n%c%c:\n", SQUARE(i));
        Print2BitBoards(ArtIsoMaskW[i], ArtIsoMaskB[i]);
        */
    }

    for (i = 0; i < nSize; i++) {
        WPawnBackwardMask[i] = BPawnBackwardMask[i] = {};
        const CSCoord Coord(static_cast<uint16_t>(i));
        const uint16_t wWidth = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[Coord.m_nLevel]);
        for (int r = static_cast<int>(Coord.m_nRank) - 1; r >= 0; r--) {
            if (Coord.m_nFile > 0) {
                WPawnBackwardMask[i].SetBit(
                    CSCoord(Coord.m_nLevel, Coord.m_nFile - 1, static_cast<uint16_t>(r))
                        .BitOffset());
            }
            if (Coord.m_nFile < (wWidth - 1)) {
                WPawnBackwardMask[i].SetBit(
                    CSCoord(Coord.m_nLevel, Coord.m_nFile + 1, static_cast<uint16_t>(r))
                        .BitOffset());
            }
        }
        for (uint16_t r = Coord.m_nRank + 1; r < wWidth; r++) {
            if (Coord.m_nFile > 0) {
                BPawnBackwardMask[i].SetBit(
                    CSCoord(Coord.m_nLevel, Coord.m_nFile - 1, r).BitOffset());
            }
            if (Coord.m_nFile < (wWidth - 1)) {
                BPawnBackwardMask[i].SetBit(
                    CSCoord(Coord.m_nLevel, Coord.m_nFile + 1, r).BitOffset());
            }
        }
    }

    for (i = 0; i < nSize; i++) {
        const CSCoord ICoord(static_cast<uint16_t>(i));
        const uint16_t wWidth = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[ICoord.m_nLevel]);
        ConnectedMask[i] = {};

        if (ICoord.m_nFile < (wWidth - 1)) {
            ConnectedMask[i].SetBit(i + 1);
            if (ICoord.m_nRank > 1) {
                ConnectedMask[i].SetBit(i - (wWidth - 1));
            }
            if (ICoord.m_nRank < (wWidth - 2)) {
                ConnectedMask[i].SetBit(i + (wWidth + 1));
            }
        }
        if (ICoord.m_nFile > 0) {
            ConnectedMask[i].SetBit(i - 1);
            if (ICoord.m_nRank > 1) {
                ConnectedMask[i].SetBit(i - (wWidth + 1));
            }
            if (ICoord.m_nRank < (wWidth - 2)) {
                ConnectedMask[i].SetBit(i + (wWidth - 1));
            }
        }
    }
}

void InitGeometry(void) {
    int rgEdge[100];
    int rgTrto[100];
    int i, j, k, l;
    const int nMaxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int nSize = static_cast<int>(CBitBoard::SIZE);
    int rgDirs[] = {1, -1, 10, -10, 9, -9, 11, -11};
    int rgDirb[] = {9, -9, 11, -11};
    int rgDirr[] = {1, -1, 10, -10};

    for (i = 0; i < 100; i++) {
        rgEdge[i] = 0;
        rgTrto[i] = 0;
    }

    for (i = 0; i < 10; i++) {
        rgEdge[i] = rgEdge[90 + i] = rgEdge[10 * i] = rgEdge[10 * i + 9] = 1;
        for (j = 0; j < 10; j++) {
            int x = i - 1;
            int y = j - 1;
            if (x >= 0 && y >= 0 && x < nMaxLevelWidth && y < nMaxLevelWidth) {
                rgTrto[i + 10 * j] = x + nMaxLevelWidth * y;
            }
        }
    }

    for (i = 0; i < nSize; i++) {
        for (j = 0; j < nSize; j++) {
            InterPath[i][j] = {};
            Ray[i][j] = {};
        }
        WPawnEPM[i] = BPawnEPM[i] = BishopEPM[i] = RookEPM[i] = QueenEPM[i] =
            {};
    }

    for (j = 0; j < 100; j++) {
        int x = rgTrto[j];
        if (rgEdge[j])
            continue;
        for (i = 0; i < 8; i++) {
            int d = rgDirs[i];
            for (k = j + d; !rgEdge[k]; k += d) {
                int y = rgTrto[k];
                for (l = j + d; l != k; l += d)
                    InterPath[x][y] |= CBitBoard::SetMask(rgTrto[l]);
                for (l = k + d; !rgEdge[l]; l += d)
                    Ray[x][y] |= CBitBoard::SetMask(rgTrto[l]);
            }
        }
        for (i = 0; i < 4; i++) {
            int d = rgDirb[i];
            for (k = j + d; !rgEdge[k]; k += d) {
                BishopEPM[x] |= CBitBoard::SetMask(rgTrto[k]);
                QueenEPM[x] |= CBitBoard::SetMask(rgTrto[k]);
            }
            d = rgDirr[i];
            for (k = j + d; !rgEdge[k]; k += d) {
                RookEPM[x] |= CBitBoard::SetMask(rgTrto[k]);
                QueenEPM[x] |= CBitBoard::SetMask(rgTrto[k]);
            }
        }
        if (!rgEdge[j + 9])
            WPawnEPM[x] |= CBitBoard::SetMask(x + 7);
        if (!rgEdge[j + 11])
            WPawnEPM[x] |= CBitBoard::SetMask(x + 9);
        if (!rgEdge[j - 9])
            BPawnEPM[x] |= CBitBoard::SetMask(x - 7);
        if (!rgEdge[j - 11])
            BPawnEPM[x] |= CBitBoard::SetMask(x - 9);
    }
}

void InitMiscMasks(void) {
    int i, j;
    const int nMaxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int nSize = static_cast<int>(CBitBoard::SIZE);

    SeventhRank[White] = SeventhRank[Black] = {};
    EighthRank[White] = EighthRank[Black] = {};
    ThirdRank[White] = ThirdRank[Black] = {};
    PrePromoRank[White] = PrePromoRank[Black] = {};

    for (i = 0; i < nMaxLevelWidth; i++) {
        RankMask[i] = {};
    }

    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int dwWidth = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (unsigned int dwRank = 0; dwRank < dwWidth; dwRank++) {
            for (unsigned int dwFile = 0; dwFile < dwWidth; dwFile++) {
                const int nSquare = static_cast<int>(
                    CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFile), static_cast<int>(dwRank)));
                RankMask[dwRank] |= CBitBoard::SetMask(nSquare);

                if (dwRank == 6) {
                    SeventhRank[White] |= CBitBoard::SetMask(nSquare);
                }
                if (dwRank == 1) {
                    SeventhRank[Black] |= CBitBoard::SetMask(nSquare);
                }
                if (dwRank == 7) {
                    EighthRank[White] |= CBitBoard::SetMask(nSquare);
                }
                if (dwRank == 0) {
                    EighthRank[Black] |= CBitBoard::SetMask(nSquare);
                }
                if (dwRank == 2) {
                    ThirdRank[White] |= CBitBoard::SetMask(nSquare);
                }
                if (dwRank == 5) {
                    ThirdRank[Black] |= CBitBoard::SetMask(nSquare);
                }
                // PrePromoRank: one step from promotion, only on promotion levels f–j (5–9)
                if (dwLevel >= 5 && dwLevel <= 9) {
                    if (dwRank == dwWidth - 2) {
                        PrePromoRank[White] |= CBitBoard::SetMask(nSquare);
                    }
                    if (dwRank == 1) {
                        PrePromoRank[Black] |= CBitBoard::SetMask(nSquare);
                    }
                }
            }
        }
    }

    for (i = 0; i < nMaxLevelWidth; i++) {
        LeftOf[i] = RightOf[i] = FarLeftOf[i] = FarRightOf[i] = {};
        for (j = i - 1; j >= 0; j--)
            LeftOf[i] |= FileMask[j];
        for (j = i - 2; j >= 0; j--)
            FarLeftOf[i] |= FileMask[j];
        for (j = i + 1; j < nMaxLevelWidth; j++)
            RightOf[i] |= FileMask[j];
        for (j = i + 2; j < nMaxLevelWidth; j++)
            FarRightOf[i] |= FileMask[j];
    }

    EdgeMask = {};

    for (i = 0; i < nMaxLevelWidth; i++) {
        EdgeMask.SetBit(ha1 + i);
        EdgeMask.SetBit(ha8 + i);
        EdgeMask.SetBit(ha1 + nMaxLevelWidth * i);
        EdgeMask.SetBit(hh1 + nMaxLevelWidth * i);
    }

    WhiteSquaresMask = BlackSquaresMask = {};
    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int dwWidth = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (unsigned int dwRank = 0; dwRank < dwWidth; dwRank++) {
            for (unsigned int dwFile = 0; dwFile < dwWidth; dwFile++) {
                const int nSquare = static_cast<int>(
                    CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFile), static_cast<int>(dwRank)));
                if (((dwRank + dwFile) & 1) == 0) {
                    BlackSquaresMask.SetBit(nSquare);
                } else {
                    WhiteSquaresMask.SetBit(nSquare);
                }
            }
        }
    }

    for (i = 0; i < nSize; i++) {
        const CSCoord Coord(i);
        const uint16_t wWidth = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[Coord.m_nLevel]);
        int nBdist = Coord.m_nRank;
        int nWdist = (wWidth - 1) - nBdist;
        CSCoord wtargetCoord(Coord.m_nLevel, Coord.m_nFile, wWidth - 1);
        CSCoord btargetCoord(Coord.m_nLevel, Coord.m_nFile, 0);

        KingSquareW[i] = KingSquareB[i] = {};
        for (j = 0; j < nSize; j++) {
            CSCoord Coord(j);
            if (KingDist(wtargetCoord, Coord) <= nWdist) {
                KingSquareW[i].SetBit(j);
            }
            if (KingDist(btargetCoord, Coord) <= nBdist) {
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
