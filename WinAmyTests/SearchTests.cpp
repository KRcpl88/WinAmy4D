#include "TestHelpers.h"

#include "time_ctl.h"

#include <string>

namespace WinAmyTests {

// Tests that exercise the full engine search (CPosition::Iterate) and verify it
// returns a *legal* best move.  These are regression tests for the class of bug
// where the engine recommended an illegal move (e.g. "Ria1bb2") for a given
// position.
TEST_CLASS(SearchTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
        // The search probes the transposition / pawn / score tables, which are
        // NULL until AllocateHT() is called.
        AllocateHT();
    }

    // Runs a shallow, deterministic search on the given EPD and asserts that the
    // engine's chosen move is non-empty, legal, and present in the legal move
    // list for the position.  Returns the engine's best move so callers can make
    // additional assertions.
    static CMove SearchAndAssertLegal(const char *pszEpd, int nMaxDepth) {
        PositionGuard Position(CPosition::CreateFromEPD(pszEpd));
        Assert::IsTrue(Position.get() != nullptr,
                       L"CreateFromEPD returned nullptr");

        // Keep the search short and deterministic: cap the iterative-deepening
        // depth and remove any wall-clock budget influence.
        setMaxSearchDepth(nMaxDepth);
        SetFixedTimePerMove(60);

        int nScore = 0;
        CMove BestMove =
            Position.get()->Iterate(&nScore, M_NONE, nullptr);

        // Restore a near-default search depth for any subsequent test.
        // setMaxSearchDepth only accepts values < MAX_TREE_SIZE - 1.
        setMaxSearchDepth(MAX_TREE_SIZE - 2);

        Assert::IsTrue(BestMove != M_NONE,
                       L"Engine returned M_NONE for a position with legal moves");

        // The chosen move must be legal in the root position.  This is exactly
        // the failure mode behind the reported "engine recommended an illegal
        // move" bug.
        Assert::IsTrue(Position.get()->LegalMove(BestMove),
                       L"Engine returned an illegal best move");

        // The chosen move must also appear in the generated legal move list.
        heap_t heap = allocate_heap();
        int nLegal = Position.get()->LegalMoves(heap);
        Assert::IsTrue(nLegal > 0, L"Position unexpectedly has no legal moves");

        bool fFound = false;
        for (unsigned int i = heap->current_section->start;
             i < heap->current_section->end; i++) {
            if (heap->data[i] == BestMove) {
                fFound = true;
                break;
            }
        }
        free_heap(heap);

        Assert::IsTrue(fFound,
                       L"Engine best move was not among the legal moves");

        return BestMove;
    }

    // Regression for the reported bug: in this position the engine previously
    // recommended an illegal move (e.g. "Ria1bb2").  The search must return a
    // legal move that is present in the legal move list.
    TEST_METHOD(EngineReturnsLegalMoveForReportedBugPosition) {
        const char *pszEpd =
            "1|2/2|3/3/3|4/4/4/4|5/5/5/5/5|6/6/6/6/6/6|ppppppp/7/7/7/7/7/"
            "PPPPPPP|rnbqkb1r/pppppppp/5n2/8/8/5N2/PPPPPPPP/RNBQKB1R|"
            "r1bq1br/ppppppp/2n4/7/2N2N1/PPPPPPP/R1BQ1BR|pppppp/5n/6/6/6/"
            "PPPPPP|5/5/5/5/5|4/4/4/4|3/3/3|2/2|1 w KQkq -";

        SearchAndAssertLegal(pszEpd, 4);
    }


    // Make sure the searchs ees the threat from nie4 catpuring the pawn at if6
    // and forcing a queen capture because the knight will check to the black king.  
    // The engine must move the black king or queen to avoid the capture
    TEST_METHOD(EngineEvadesForcedQueenCapture) {
        const char *pszEpd =
            "1|2/2|3/3/3|4/4/4/4|5/5/5/5/5|6/6/6/6/6/6|ppppppp/7/5n1/7/7/"
            "P1N4/1PPPPPP|rnbqkb1r/pppppppp/5n2/8/4n3/5N2/PPPPPPPP/R1BQKB1R|"
            "r1bq1br/ppppppp/7/4N2/5N1/PPPPPPP/R1BQ1BR|pppppp/6/6/6/6/"
            "PPPPPP|5/5/5/5/5|4/4/4/4|3/3/3|2/2|1 b KQkq -";

        CMove Result = SearchAndAssertLegal(pszEpd, 4);
        
        char rgMsg[256]{0};

        _snprintf_s(rgMsg, sizeof(rgMsg), "Best move: %d,%d,%d to %d,%d,%d", Result.GetFromCoord().m_nLevel, Result.GetFromCoord().m_nFile, Result.GetFromCoord().m_nRank,
                   Result.GetToCoord().m_nLevel, Result.GetToCoord().m_nFile, Result.GetToCoord().m_nRank);

        Logger::WriteMessage(rgMsg);

        Assert::IsTrue(Result.GetFromCoord() == CSCoord(8,3,6) ||
                       Result.GetFromCoord() == CSCoord(7,4,7) ,
                       L"Engine failed to evade forced queen capture, move was scoord");
    }


    // White must sacrifice the rook to save the queen from capture by the knight, which would check the black king.
    TEST_METHOD(EngineSacrificeRookEvadesForcedQueenCapture) {
        const char *pszEpd =
            "K|2/2|3/3/3|4/4/4/4|5/3Q1/5/5/n4|6/6/6/6/6/6|7/7/7/7/7/7/7|"
            "8/8/8/8/8/8/2R5/8|7/7/7/7/7/7/7|6/6/6/6/6/6|5/5/5/5/5|"
            "4/4/4/4|n2/3/r2|bb/2|k w - -";

        CMove Result = SearchAndAssertLegal(pszEpd, 4);
        
        char rgMsg[256]{0};

        _snprintf_s(rgMsg, sizeof(rgMsg), "Best move: %d,%d,%d to %d,%d,%d", Result.GetFromCoord().m_nLevel, Result.GetFromCoord().m_nFile, Result.GetFromCoord().m_nRank,
                   Result.GetToCoord().m_nLevel, Result.GetToCoord().m_nFile, Result.GetToCoord().m_nRank);

        Logger::WriteMessage(rgMsg);

        /*

        Assert::IsTrue(Result.GetFromCoord() == CSCoord(8,3,6) ||
                       Result.GetFromCoord() == CSCoord(7,4,7) ,
                       L"Engine failed to evade forced queen capture, move was scoord");*/
    }


    // Sanity check on the standard initial position: the engine must produce a
    // legal opening move.
    TEST_METHOD(EngineReturnsLegalMoveForInitialPosition) {
        PositionGuard Position(CPosition::Initial());

        setMaxSearchDepth(4);
        SetFixedTimePerMove(60);

        int nScore = 0;
        CMove BestMove = Position.get()->Iterate(&nScore, M_NONE, nullptr);

        setMaxSearchDepth(MAX_TREE_SIZE - 2);

        Assert::IsTrue(BestMove != M_NONE,
                       L"Engine returned M_NONE for the initial position");
        Assert::IsTrue(Position.get()->LegalMove(BestMove),
                       L"Engine returned an illegal move for the initial position");
    }
};

} // namespace WinAmyTests
