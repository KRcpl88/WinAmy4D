#include "TestHelpers.h"
#include "heap.h"
#include "position.h"

#include <string>
#include <vector>

namespace WinAmyTests {

// Regression tests for 4D level-awareness in SAN notation, check detection,
// and pawn double-push legality.  Each played game is driven by a fixed,
// deterministic pseudo-random sequence so the tests are reproducible.
TEST_CLASS(SanAndCheckTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    static std::vector<CMove> CollectLegalMoves(CPosition *pPosition) {
        heap_t hHeap = allocate_heap();
        pPosition->LegalMoves(hHeap);
        std::vector<CMove> rgMoves;
        for (unsigned int nIndex = hHeap->current_section->start;
             nIndex < hHeap->current_section->end; nIndex++) {
            rgMoves.push_back(hHeap->data[nIndex]);
        }
        free_heap(hHeap);
        return rgMoves;
    }

    // SAN must round-trip through ParseSAN for every legal move.  In 4D two
    // same-type pieces (or pawns) can share file and rank but sit on different
    // levels; the SAN disambiguator must encode the source level so the move
    // parses back unambiguously.  The test also asserts that such level-only
    // ambiguous configurations are actually encountered, so it genuinely
    // exercises the regression.
    TEST_METHOD(SanRoundTripsForEveryLegalMove) {
        uint32_t dwRng = 0x1234567u;
        auto Next = [&dwRng]() {
            dwRng = dwRng * 1664525u + 1013904223u;
            return dwRng;
        };
        int nLevelDisambiguations = 0;
        for (int nGame = 0; nGame < 25; nGame++) {
            CPosition *pPosition = CPosition::Initial();
            for (int nPly = 0; nPly < 35; nPly++) {
                std::vector<CMove> rgMoves = CollectLegalMoves(pPosition);
                if (rgMoves.empty()) {
                    break;
                }
                for (CMove move : rgMoves) {
                    char szSan[32];
                    std::string strSan = pPosition->SAN(move, szSan);
                    CMove parsed = pPosition->ParseSAN(strSan.c_str());
                    Assert::IsTrue(
                        parsed != M_NONE,
                        L"ParseSAN returned M_NONE for a generated SAN");
                    Assert::IsTrue(
                        parsed.GetFromToIndex() == move.GetFromToIndex(),
                        L"SAN did not round-trip to the same move");
                }
                for (size_t i = 0; i < rgMoves.size(); i++) {
                    for (size_t j = i + 1; j < rgMoves.size(); j++) {
                        const CSCoord& fromI = rgMoves[i].GetFromCoord();
                        const CSCoord& fromJ = rgMoves[j].GetFromCoord();
                        const CSCoord& toI = rgMoves[i].GetToCoord();
                        const CSCoord& toJ = rgMoves[j].GetToCoord();
                        if (toI.BitOffset() != toJ.BitOffset()) {
                            continue;
                        }
                        if (TYPE(pPosition->GetPiece(fromI.BitOffset())) !=
                            TYPE(pPosition->GetPiece(fromJ.BitOffset()))) {
                            continue;
                        }
                        if (fromI.m_nFile == fromJ.m_nFile &&
                            fromI.m_nRank == fromJ.m_nRank &&
                            fromI.m_nLevel != fromJ.m_nLevel) {
                            nLevelDisambiguations++;
                        }
                    }
                }
                pPosition->DoMove(rgMoves[Next() % rgMoves.size()]);
            }
            CPosition::Free(pPosition);
        }
        Assert::IsTrue(
            nLevelDisambiguations > 0,
            L"test did not exercise any level-only disambiguation cases");
    }

    // IsCheckingMove must agree with actually making the move and testing for
    // check, for every legal move.  The old heuristic ignored cross-level
    // attacks (and many discovered checks), so it disagreed frequently.
    TEST_METHOD(IsCheckingMoveMatchesActualCheck) {
        uint32_t dwRng = 0x89abcdefu;
        auto Next = [&dwRng]() {
            dwRng = dwRng * 1664525u + 1013904223u;
            return dwRng;
        };
        int nChecks = 0;
        for (int nGame = 0; nGame < 25; nGame++) {
            CPosition *pPosition = CPosition::Initial();
            for (int nPly = 0; nPly < 35; nPly++) {
                std::vector<CMove> rgMoves = CollectLegalMoves(pPosition);
                if (rgMoves.empty()) {
                    break;
                }
                for (CMove move : rgMoves) {
                    bool fReports = pPosition->IsCheckingMove(move);
                    pPosition->DoMove(move);
                    bool fActual = pPosition->InCheck(pPosition->GetTurn());
                    pPosition->UndoMove(move);
                    if (fActual) {
                        nChecks++;
                    }
                    Assert::AreEqual(
                        fActual, fReports,
                        L"IsCheckingMove disagreed with the actual check status");
                }
                pPosition->DoMove(rgMoves[Next() % rgMoves.size()]);
            }
            CPosition::Free(pPosition);
        }
        Assert::IsTrue(nChecks > 0, L"test did not exercise any checking moves");
    }

    // A double-push flagged move whose pawn is not on the main board's home
    // rank must be rejected by LegalMove.  Double pushes only exist on the
    // main level's home rank, so legality must check the source level/rank and
    // not merely that the two squares ahead are empty.
    TEST_METHOD(LegalMoveRejectsDoublePushOffHomeRank) {
        char szEpd[] = "4k3/8/8/8/8/4P3/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(szEpd));
        CMove move = MakeMainBoardMove(he3, he5, M_PAWND);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    // The matching legal double push from the home rank is still accepted.
    TEST_METHOD(LegalMoveAcceptsDoublePushFromHomeRank) {
        char szEpd[] = "4k3/8/8/8/8/8/4P3/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(szEpd));
        CMove move = MakeMainBoardMove(he2, he4, M_PAWND);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    // Regression for the reported "compute strategy recommended the illegal
    // move Rca3" bug.  The search was finding a legitimate cross-level rook
    // move, but the level-blind SAN rendered it as a string whose square no
    // rook can reach.  Now every legal move in this exact position must
    // round-trip through SAN, and the bogus "Rca3" must not resolve to any
    // legal move.
    TEST_METHOD(StrategyEpdSanRoundTripsAndRejectsRca3) {
        const char *szEpd =
            "1|2/1r|3/3/3|4/4/4/4|4R/5/5/5/5|6/6/6/6/6/4N1|"
            "ppppppp/7/7/7/7/2NPN2/PPPQPPP|"
            "r1bq1rk1/p1pp1ppp/1pnbp3/8/8/8/PPPPPPPP/2BQ1B1R|"
            "1nbqb1r/ppppppp/4n2/7/7/PP1PPPP/1NBKB1R|"
            "pppppp/6/6/6/P5/1PPPPP|5/5/5/5/5|4/4/4/4|3/3/3|2/2|1 w - -";
        PositionGuard position(CPosition::CreateFromEPD(szEpd));
        CPosition *pPosition = position.get();

        std::vector<CMove> rgMoves = CollectLegalMoves(pPosition);
        Assert::IsTrue(rgMoves.size() > 0, L"position produced no legal moves");
        for (CMove move : rgMoves) {
            char szSan[32];
            std::string strSan = pPosition->SAN(move, szSan);
            CMove parsed = pPosition->ParseSAN(strSan.c_str());
            Assert::IsTrue(parsed != M_NONE,
                           L"ParseSAN returned M_NONE for a generated SAN");
            Assert::IsTrue(parsed.GetFromToIndex() == move.GetFromToIndex(),
                           L"SAN did not round-trip to the same move");
            Assert::IsTrue(strSan != "Rca3",
                           L"no legal move should be rendered as Rca3");
        }

        // ca3 (level c, file a, rank 3) is unreachable by any rook here.
        CSCoord targetCoord(2, 0, 2);
        uint16_t wTarget = targetCoord.BitOffset();
        for (CMove move : rgMoves) {
            Assert::IsFalse(move.GetToCoord().BitOffset() == wTarget,
                            L"no legal move should target the unreachable ca3");
        }
        Assert::IsTrue(pPosition->ParseSAN("Rca3") == M_NONE,
                       L"Rca3 must not parse to a legal move");
    }
};

} // namespace WinAmyTests
