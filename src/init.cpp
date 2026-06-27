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
    const CBitBoard allOnes = ~CBitBoard();
    ShiftUpMask = ShiftDownMask = ShiftLeftMask = ShiftRightMask = allOnes;
    for (unsigned int dwI = 0; dwI < CBitBoard::MAX_LEVEL_WIDTH; dwI++) {
        ShiftUpMask &= CBitBoard::ClrMask(static_cast<uint16_t>(dwI));
        ShiftDownMask &= CBitBoard::ClrMask(
            static_cast<uint16_t>(CBitBoard::SIZE - CBitBoard::MAX_LEVEL_WIDTH + dwI));
        ShiftRightMask &= CBitBoard::ClrMask(static_cast<uint16_t>(
            CBitBoard::MAX_LEVEL_WIDTH * dwI + (CBitBoard::MAX_LEVEL_WIDTH - 1)));
        ShiftLeftMask &= CBitBoard::ClrMask(
            static_cast<uint16_t>(CBitBoard::MAX_LEVEL_WIDTH * dwI));
    }
}

void PrintBitBoard(CBitBoard x) {
    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int width = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (int nRank = static_cast<int>(width) - 1; nRank >= 0; nRank--) {
            for (unsigned int file = 0; file < width; file++) {
                int nK =
                    static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(file), nRank));
                if (x.TstBit(nK))
                    Print(0, "*");
                else
                    Print(0, ".");
            }
            Print(0, "\n");
        }
    }
}

void InitPawnMasks(void) {
    int nI;
    const int nMaxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int nSize = static_cast<int>(CBitBoard::SIZE);

    for (nI = 0; nI < nMaxLevelWidth; nI++) {
        FileMask[nI] = {};
        IsoMask[nI] = {};
    }

    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int width = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (unsigned int file = 0; file < width; file++) {
            for (unsigned int dwRank = 0; dwRank < width; dwRank++) {
                const uint16_t wSquare =
                    CSCoord(static_cast<uint16_t>(dwLevel), static_cast<uint16_t>(file),
                            static_cast<uint16_t>(dwRank))
                        .BitOffset();
                FileMask[file] |= CBitBoard::SetMask(wSquare);
                if (file > 0) {
                    IsoMask[file] |= CBitBoard::SetMask(
                        CSCoord(static_cast<uint16_t>(dwLevel),
                                static_cast<uint16_t>(file - 1),
                                static_cast<uint16_t>(dwRank))
                            .BitOffset());
                }
                if ((file + 1) < width) {
                    IsoMask[file] |= CBitBoard::SetMask(
                        CSCoord(static_cast<uint16_t>(dwLevel),
                                static_cast<uint16_t>(file + 1),
                                static_cast<uint16_t>(dwRank))
                            .BitOffset());
                }
            }
#ifdef DEBUG
            PrintBitBoard(IsoMask[file]);
#endif
        }
    }
    for (nI = 0; nI < nSize; nI++) {
        ForwardRayW[nI] = ForwardRayB[nI] = {};
        const CSCoord coord(static_cast<uint16_t>(nI));
        const uint16_t width = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[coord.m_nLevel]);
        for (uint16_t wR = coord.m_nRank + 1; wR < width; wR++) {
            ForwardRayW[nI].SetBit(
                CSCoord(coord.m_nLevel, coord.m_nFile, wR).BitOffset());
        }
        for (int nR = static_cast<int>(coord.m_nRank) - 1; nR >= 0; nR--) {
            ForwardRayB[nI].SetBit(
                CSCoord(coord.m_nLevel, coord.m_nFile, static_cast<uint16_t>(nR))
                    .BitOffset());
        }
#ifdef DEBUG
        PrintBitBoard(ForwardRayW[nI]);
        PrintBitBoard(ForwardRayB[nI]);
#endif
    }
    for (nI = 0; nI < nSize; nI++) {
        const CSCoord coord(nI);
        const uint16_t width = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[coord.m_nLevel]);
        PassedMaskW[nI] = ForwardRayW[nI];
        if (coord.m_nFile > 0)
            PassedMaskW[nI] |= ForwardRayW[nI - 1];
        if (coord.m_nFile < (width - 1))
            PassedMaskW[nI] |= ForwardRayW[nI + 1];
        PassedMaskB[nI] = ForwardRayB[nI];
        if (coord.m_nFile > 0)
            PassedMaskB[nI] |= ForwardRayB[nI - 1];
        if (coord.m_nFile < (width - 1))
            PassedMaskB[nI] |= ForwardRayB[nI + 1];
        /* PrintBitBoard(PassedMaskW[i]); */
        /* PrintBitBoard(PassedMaskB[i]); */
        OutpostMaskW[nI] = OutpostMaskB[nI] = {};
        if (coord.m_nFile > 0) {
            OutpostMaskW[nI] |= ForwardRayW[nI - 1];
            OutpostMaskB[nI] |= ForwardRayB[nI - 1];
        }
        if (coord.m_nFile < (width - 1)) {
            OutpostMaskW[nI] |= ForwardRayW[nI + 1];
            OutpostMaskB[nI] |= ForwardRayB[nI + 1];
        }
        /*
        printf("\n%c%c:\n", SQUARE(i));
        Print2BitBoards(ArtIsoMaskW[i], ArtIsoMaskB[i]);
        */
    }

    for (nI = 0; nI < nSize; nI++) {
        WPawnBackwardMask[nI] = BPawnBackwardMask[nI] = {};
        const CSCoord coord(static_cast<uint16_t>(nI));
        const uint16_t width = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[coord.m_nLevel]);
        for (int nR = static_cast<int>(coord.m_nRank) - 1; nR >= 0; nR--) {
            if (coord.m_nFile > 0) {
                WPawnBackwardMask[nI].SetBit(
                    CSCoord(coord.m_nLevel, coord.m_nFile - 1, static_cast<uint16_t>(nR))
                        .BitOffset());
            }
            if (coord.m_nFile < (width - 1)) {
                WPawnBackwardMask[nI].SetBit(
                    CSCoord(coord.m_nLevel, coord.m_nFile + 1, static_cast<uint16_t>(nR))
                        .BitOffset());
            }
        }
        for (uint16_t wR = coord.m_nRank + 1; wR < width; wR++) {
            if (coord.m_nFile > 0) {
                BPawnBackwardMask[nI].SetBit(
                    CSCoord(coord.m_nLevel, coord.m_nFile - 1, wR).BitOffset());
            }
            if (coord.m_nFile < (width - 1)) {
                BPawnBackwardMask[nI].SetBit(
                    CSCoord(coord.m_nLevel, coord.m_nFile + 1, wR).BitOffset());
            }
        }
    }

    for (nI = 0; nI < nSize; nI++) {
        const CSCoord iCoord(static_cast<uint16_t>(nI));
        const uint16_t width = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[iCoord.m_nLevel]);
        ConnectedMask[nI] = {};

        if (iCoord.m_nFile < (width - 1)) {
            ConnectedMask[nI].SetBit(nI + 1);
            if (iCoord.m_nRank > 1) {
                ConnectedMask[nI].SetBit(nI - (width - 1));
            }
            if (iCoord.m_nRank < (width - 2)) {
                ConnectedMask[nI].SetBit(nI + (width + 1));
            }
        }
        if (iCoord.m_nFile > 0) {
            ConnectedMask[nI].SetBit(nI - 1);
            if (iCoord.m_nRank > 1) {
                ConnectedMask[nI].SetBit(nI - (width + 1));
            }
            if (iCoord.m_nRank < (width - 2)) {
                ConnectedMask[nI].SetBit(nI + (width - 1));
            }
        }
    }
}

void InitGeometry(void) {
    int rgEdge[100];
    int rgTrto[100];
    int nI, nJ, nK, nL;
    const int nMaxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int nSize = static_cast<int>(CBitBoard::SIZE);
    int rgDirs[] = {1, -1, 10, -10, 9, -9, 11, -11};
    int rgDirb[] = {9, -9, 11, -11};
    int rgDirr[] = {1, -1, 10, -10};

    for (nI = 0; nI < 100; nI++) {
        rgEdge[nI] = 0;
        rgTrto[nI] = 0;
    }

    for (nI = 0; nI < 10; nI++) {
        rgEdge[nI] = rgEdge[90 + nI] = rgEdge[10 * nI] = rgEdge[10 * nI + 9] = 1;
        for (nJ = 0; nJ < 10; nJ++) {
            int nX = nI - 1;
            int nY = nJ - 1;
            if (nX >= 0 && nY >= 0 && nX < nMaxLevelWidth && nY < nMaxLevelWidth) {
                rgTrto[nI + 10 * nJ] = nX + nMaxLevelWidth * nY;
            }
        }
    }

    for (nI = 0; nI < nSize; nI++) {
        for (nJ = 0; nJ < nSize; nJ++) {
            InterPath[nI][nJ] = {};
            Ray[nI][nJ] = {};
        }
        WPawnEPM[nI] = BPawnEPM[nI] = BishopEPM[nI] = RookEPM[nI] = QueenEPM[nI] =
            {};
    }

    for (nJ = 0; nJ < 100; nJ++) {
        int nX = rgTrto[nJ];
        if (rgEdge[nJ])
            continue;
        for (nI = 0; nI < 8; nI++) {
            int nD = rgDirs[nI];
            for (nK = nJ + nD; !rgEdge[nK]; nK += nD) {
                int nY = rgTrto[nK];
                for (nL = nJ + nD; nL != nK; nL += nD)
                    InterPath[nX][nY] |= CBitBoard::SetMask(rgTrto[nL]);
                for (nL = nK + nD; !rgEdge[nL]; nL += nD)
                    Ray[nX][nY] |= CBitBoard::SetMask(rgTrto[nL]);
            }
        }
        for (nI = 0; nI < 4; nI++) {
            int nD = rgDirb[nI];
            for (nK = nJ + nD; !rgEdge[nK]; nK += nD) {
                BishopEPM[nX] |= CBitBoard::SetMask(rgTrto[nK]);
                QueenEPM[nX] |= CBitBoard::SetMask(rgTrto[nK]);
            }
            nD = rgDirr[nI];
            for (nK = nJ + nD; !rgEdge[nK]; nK += nD) {
                RookEPM[nX] |= CBitBoard::SetMask(rgTrto[nK]);
                QueenEPM[nX] |= CBitBoard::SetMask(rgTrto[nK]);
            }
        }
        if (!rgEdge[nJ + 9])
            WPawnEPM[nX] |= CBitBoard::SetMask(nX + 7);
        if (!rgEdge[nJ + 11])
            WPawnEPM[nX] |= CBitBoard::SetMask(nX + 9);
        if (!rgEdge[nJ - 9])
            BPawnEPM[nX] |= CBitBoard::SetMask(nX - 7);
        if (!rgEdge[nJ - 11])
            BPawnEPM[nX] |= CBitBoard::SetMask(nX - 9);
    }
}

void InitMiscMasks(void) {
    int nI, nJ;
    const int nMaxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    const int nSize = static_cast<int>(CBitBoard::SIZE);

    SeventhRank[White] = SeventhRank[Black] = {};
    EighthRank[White] = EighthRank[Black] = {};
    ThirdRank[White] = ThirdRank[Black] = {};
    PrePromoRank[White] = PrePromoRank[Black] = {};

    for (nI = 0; nI < nMaxLevelWidth; nI++) {
        RankMask[nI] = {};
    }

    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int width = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (unsigned int dwRank = 0; dwRank < width; dwRank++) {
            for (unsigned int file = 0; file < width; file++) {
                const int nSquare = static_cast<int>(
                    CSCoord(static_cast<int>(dwLevel), static_cast<int>(file), static_cast<int>(dwRank)));
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
                    if (dwRank == width - 2) {
                        PrePromoRank[White] |= CBitBoard::SetMask(nSquare);
                    }
                    if (dwRank == 1) {
                        PrePromoRank[Black] |= CBitBoard::SetMask(nSquare);
                    }
                }
            }
        }
    }

    for (nI = 0; nI < nMaxLevelWidth; nI++) {
        LeftOf[nI] = RightOf[nI] = FarLeftOf[nI] = FarRightOf[nI] = {};
        for (nJ = nI - 1; nJ >= 0; nJ--)
            LeftOf[nI] |= FileMask[nJ];
        for (nJ = nI - 2; nJ >= 0; nJ--)
            FarLeftOf[nI] |= FileMask[nJ];
        for (nJ = nI + 1; nJ < nMaxLevelWidth; nJ++)
            RightOf[nI] |= FileMask[nJ];
        for (nJ = nI + 2; nJ < nMaxLevelWidth; nJ++)
            FarRightOf[nI] |= FileMask[nJ];
    }

    EdgeMask = {};

    for (nI = 0; nI < nMaxLevelWidth; nI++) {
        EdgeMask.SetBit(ha1 + nI);
        EdgeMask.SetBit(ha8 + nI);
        EdgeMask.SetBit(ha1 + nMaxLevelWidth * nI);
        EdgeMask.SetBit(hh1 + nMaxLevelWidth * nI);
    }

    WhiteSquaresMask = BlackSquaresMask = {};
    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int width = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (unsigned int dwRank = 0; dwRank < width; dwRank++) {
            for (unsigned int file = 0; file < width; file++) {
                const int nSquare = static_cast<int>(
                    CSCoord(static_cast<int>(dwLevel), static_cast<int>(file), static_cast<int>(dwRank)));
                if (((dwRank + file) & 1) == 0) {
                    BlackSquaresMask.SetBit(nSquare);
                } else {
                    WhiteSquaresMask.SetBit(nSquare);
                }
            }
        }
    }

    for (nI = 0; nI < nSize; nI++) {
        const CSCoord coord(nI);
        const uint16_t width = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[coord.m_nLevel]);
        int bdist = coord.m_nRank;
        int wdist = (width - 1) - bdist;
        CSCoord wtargetCoord(coord.m_nLevel, coord.m_nFile, width - 1);
        CSCoord btargetCoord(coord.m_nLevel, coord.m_nFile, 0);

        KingSquareW[nI] = KingSquareB[nI] = {};
        for (nJ = 0; nJ < nSize; nJ++) {
            CSCoord coord(nJ);
            if (KingDist(wtargetCoord, coord) <= wdist) {
                KingSquareW[nI].SetBit(nJ);
            }
            if (KingDist(btargetCoord, coord) <= bdist) {
                KingSquareB[nI].SetBit(nJ);
            }
        }
    }

    NotAFileMask = NotHFileMask = {};
    for (nI = 0; nI < 7; nI++) {
        NotAFileMask |= FileMask[nI + 1];
        NotHFileMask |= FileMask[nI];
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
