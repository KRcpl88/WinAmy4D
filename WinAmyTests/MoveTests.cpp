#include "TestHelpers.h"
#include "heap.h"
#include <vector>

namespace WinAmyTests {

TEST_CLASS(MoveTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(DoMoveAndUndoMoveRestorePosition) {
        PositionGuard position(CPosition::Initial());
        PositionGuard snapshot(CPosition::Clone(position.get()));

        const CMove move = MakeMainBoardMove(he2, he4, M_PAWND);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->GetPiece(MainBoardOffset(he4)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(he2)));
        Assert::AreEqual((int)Black, (int)position.get()->GetTurn());
        Assert::AreEqual(1, (int)position.get()->GetPly());

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoNullAndUndoNullRestorePositionFields) {
        // Position with en passant available on hd6.
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        position.get()->DoNull();
        Assert::AreEqual((int)Black, (int)position.get()->GetTurn());
        Assert::IsFalse(position.get()->GetEnPassant().IsValid());

        position.get()->UndoNull();
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(RecalcAttacksRebuildsAtkSetDerivedData) {
        // Position with a white bishop on hd5.
        char epd[] = "4k3/8/8/3B4/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        position.get()->RecalcAttacks();

        CBitBoard occupied = position.get()->GetMask(White, 0) | position.get()->GetMask(Black, 0);
        CBitBoard expectedBishopAttacks = ComputeSlidingAttacks(MainBoardCoord(hd5), Bishop, occupied);

        Assert::IsTrue(position.get()->GetAtkTo(MainBoardOffset(hd5)) == expectedBishopAttacks);
        Assert::IsTrue((position.get()->GetAtkFr(MainBoardOffset(he6)) &
                        CBitBoard::SetMask(MainBoardOffset(hd5)))
                           .IsNotEmpty());
        Assert::IsTrue((position.get()->GetAtkFr(MainBoardOffset(hc4)) &
                        CBitBoard::SetMask(MainBoardOffset(hd5)))
                           .IsNotEmpty());
    }

    TEST_METHOD(DoMoveCaptureRemovesCapturedPiece) {
        // White knight captures black pawn
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(hd4, he6, M_CAPTURE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Knight, (int)position.get()->GetPiece(MainBoardOffset(he6)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(hd4)));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveEnPassantCapturesCorrectly) {
        // White pawn on he5, black pawn just moved hd7-hd5, en passant on hd6
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(he5, hd6, M_ENPASSANT);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->GetPiece(MainBoardOffset(hd6)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(he5)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(hd5)));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveShortCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_G1, M_SCASTLE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)King, (int)position.get()->GetPiece(CASTLE_G1));
        Assert::AreEqual((int)Rook, (int)position.get()->GetPiece(CASTLE_F1));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(CASTLE_E1));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(CASTLE_H1));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveLongCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/R3K3 w Q -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_C1, M_LCASTLE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)King, (int)position.get()->GetPiece(CASTLE_C1));
        Assert::AreEqual((int)Rook, (int)position.get()->GetPiece(CASTLE_D1));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(CASTLE_E1));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(CASTLE_A1));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMovePromotionChangesType) {
        char epd[] = "7k/4P3/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(
            he7, he8,
            static_cast<int>(static_cast<uint32_t>(Queen) << M_PROMOTION_OFFSET));
        position.get()->DoMove(move);

        Assert::AreEqual((int)Queen, (int)position.get()->GetPiece(MainBoardOffset(he8)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(he7)));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(CMoveFlagQueriesReflectConstructorFlags) {
        const CMove move = MakeMainBoardPromotion(
            he2, he4, Queen, M_CAPTURE | M_PAWND | M_ENPASSANT | M_SCASTLE | M_LCASTLE);

        Assert::IsTrue(move.IsCapture());
        Assert::IsTrue(move.IsPawnDoublePush());
        Assert::IsTrue(move.IsEnPassant());
        Assert::IsTrue(move.IsShortCastle());
        Assert::IsTrue(move.IsLongCastle());
        Assert::IsTrue(move.IsCastle());
        Assert::IsTrue(move.HasPromotion());
        Assert::AreEqual((int)Queen, move.GetPromotionType());
        Assert::IsTrue(move.IsTactical());
    }

    TEST_METHOD(CMoveMutatorsSetAndClearFlags) {
        CMove move = MakeMainBoardMove(ha2, ha3, 0);

        Assert::IsFalse(move.IsCapture());
        Assert::IsFalse(move.IsShortCastle());
        Assert::IsFalse(move.IsLongCastle());
        Assert::IsFalse(move.IsCastle());
        Assert::IsFalse(move.IsPawnDoublePush());
        Assert::IsFalse(move.IsEnPassant());
        Assert::IsFalse(move.IsTactical());

        move.SetCapture();
        move.SetShortCastle();
        move.SetLongCastle();
        move.SetPawnDoublePush();
        move.SetEnPassant();
        Assert::IsTrue(move.IsCapture());
        Assert::IsTrue(move.IsShortCastle());
        Assert::IsTrue(move.IsLongCastle());
        Assert::IsTrue(move.IsCastle());
        Assert::IsTrue(move.IsPawnDoublePush());
        Assert::IsTrue(move.IsEnPassant());
        Assert::IsTrue(move.IsTactical());

        move.SetCapture(false);
        move.SetShortCastle(false);
        move.SetLongCastle(false);
        move.SetPawnDoublePush(false);
        move.SetEnPassant(false);
        Assert::IsFalse(move.IsCapture());
        Assert::IsFalse(move.IsShortCastle());
        Assert::IsFalse(move.IsLongCastle());
        Assert::IsFalse(move.IsCastle());
        Assert::IsFalse(move.IsPawnDoublePush());
        Assert::IsFalse(move.IsEnPassant());
        Assert::IsFalse(move.IsTactical());
    }

    TEST_METHOD(CMovePromotionMutatorsControlPromotionFlags) {
        CMove move = MakeMainBoardMove(hh7, hh8, 0);
        Assert::IsFalse(move.HasPromotion());
        Assert::AreEqual(0, move.GetPromotionType());
        Assert::IsFalse(move.IsTactical());

        move.SetPromotionType(Rook);
        Assert::IsTrue(move.HasPromotion());
        Assert::AreEqual((int)Rook, move.GetPromotionType());
        Assert::IsTrue(move.IsTactical());

        move.ClearPromotion();
        Assert::IsFalse(move.HasPromotion());
        Assert::AreEqual(0, move.GetPromotionType());
        Assert::IsFalse(move.IsTactical());
    }

    TEST_METHOD(CMoveEqualityAndInequalityCompareMoveBits) {
        const CMove moveA = MakeMainBoardMove(hb1, hc3, M_CAPTURE);
        const CMove moveB = MakeMainBoardMove(hb1, hc3, M_CAPTURE);
        const CMove moveDifferentFlags = MakeMainBoardMove(hb1, hc3, 0);
        const CMove moveDifferentTo = MakeMainBoardMove(hb1, ha3, M_CAPTURE);

        Assert::IsTrue(moveA == moveB);
        Assert::IsFalse(moveA != moveB);
        Assert::IsTrue(moveA != moveDifferentFlags);
        Assert::IsTrue(moveA != moveDifferentTo);
    }

    TEST_METHOD(CMoveFromToIndexMatchesSquareEncoding) {
        const CMove move = MakeMainBoardMove(hc2, hg7, 0);
        const SFromToIndex expected(MainBoardOffset(hc2), MainBoardOffset(hg7));
        Assert::AreEqual(expected.nFromOffset, move.GetFromCoord().BitOffset());
        Assert::AreEqual(expected.nToOffset, move.GetToCoord().BitOffset());
    }

    TEST_METHOD(MFromAndMToDecodeFromScooordBitfields) {
        const CSCoord fromSquare(MAIN_LEVEL, 4, 1); // he2 on main board
        const CSCoord toSquare(MAIN_LEVEL, 4, 3);   // he4 on main board
        const CMove move = MakeMainBoardMove(static_cast<int>(fromSquare), static_cast<int>(toSquare), M_PAWND);

        Assert::AreEqual(fromSquare.m_nLevel, move.GetFromCoord().m_nLevel);
        Assert::AreEqual(fromSquare.m_nFile, move.GetFromCoord().m_nFile);
        Assert::AreEqual(fromSquare.m_nRank, move.GetFromCoord().m_nRank);
        Assert::AreEqual(toSquare.m_nLevel, move.GetToCoord().m_nLevel);
        Assert::AreEqual(toSquare.m_nFile, move.GetToCoord().m_nFile);
        Assert::AreEqual(toSquare.m_nRank, move.GetToCoord().m_nRank);
    }

    // Regression: a pawn sitting on the last rank of a narrow (non-main) level
    // must not push forward, because the square one rank ahead does not exist
    // on that level.  Previously GenFrom constructed an out-of-range CSCoord
    // (e.g. level 6 / file 0 / rank 7 on a 7-wide level), throwing
    // std::out_of_range from CSCoord::Validate.
    TEST_METHOD(GenFromPawnOnLastRankOfNarrowLevelDoesNotThrow) {
        // White pawn on level g (level 6, 7 wide) at file 0 / rank 6 (top rank).
        const char *pszEpd =
            "1|2/2|3/3/3|4/4/4/4|5/5/5/5/5|6/6/6/6/6/6|P6/7/7/7/7/7/7|"
            "4k3/8/8/8/8/8/8/4K3| w - -";
        PositionGuard position(CPosition::CreateFromEPD(pszEpd));

        const CSCoord pawnCoord(6, 0, 6);
        Assert::AreEqual((int)Pawn,
                         (int)position.get()->GetPiece(pawnCoord.BitOffset()));

        // Generating all pseudo-legal moves must not throw and must not produce
        // any move originating from the stranded pawn.
        heap_t heap = allocate_heap();
        position.get()->PLegalMoves(heap);

        for (unsigned int i = heap->current_section->start;
             i < heap->current_section->end; i++) {
            Assert::AreNotEqual(pawnCoord.BitOffset(),
                                heap->data[i].GetFromCoord().BitOffset(),
                                L"Pawn on last rank of narrow level must not move forward");
        }
        free_heap(heap);
    }

    // Closes the DoMove/UndoMove test gap left by the king-safety self-play
    // repro (which only ever calls DoMove). Play a long pseudo-random legal
    // game recording every move, then UndoMove each one in reverse order and
    // assert the position is byte-for-byte identical to a snapshot taken at
    // each ply along the way. This exercises UndoMove across captures,
    // en passant, castling and promotions over many random positions.
    TEST_METHOD(DoMoveUndoMoveRoundTripsOverRandomGame) {
        uint32_t nRng = 0xC0FFEEu;
        auto Rand = [&nRng]() {
            nRng = nRng * 1664525u + 1013904223u;
            return nRng;
        };

        for (int nGame = 0; nGame < 20; nGame++) {
            nRng = 0x1000u + nGame * 2654435761u;
            PositionGuard position(CPosition::Initial());

            std::vector<CMove> rgMovesPlayed;
            std::vector<CPosition *> rgSnapshots;

            for (int nPly = 0; nPly < 60; nPly++) {
                PositionGuard snapshot(CPosition::Clone(position.get()));

                heap_t heap = allocate_heap();
                push_section(heap);
                position.get()->LegalMoves(heap);
                std::vector<CMove> rgMoves;
                for (unsigned int i = heap->current_section->start;
                     i < heap->current_section->end; ++i) {
                    rgMoves.push_back(heap->data[i]);
                }
                free_heap(heap);
                if (rgMoves.empty()) {
                    break;
                }

                CMove move = rgMoves[Rand() % rgMoves.size()];

                position.get()->DoMove(move);
                rgMovesPlayed.push_back(move);
                rgSnapshots.push_back(CPosition::Clone(snapshot.get()));
            }

            // Unwind the whole game one move at a time, asserting after each
            // UndoMove that we exactly recover the snapshot taken before the
            // corresponding DoMove.
            for (int i = (int)rgMovesPlayed.size() - 1; i >= 0; i--) {
                position.get()->UndoMove(rgMovesPlayed[i]);
                AssertPositionsEqual(position.get(), rgSnapshots[i]);
                CPosition::Free(rgSnapshots[i]);
            }
        }
    }

    // Regression: search can push the game-log well past the initial allocation
    // (128 plies) before unwinding. Verify that repeated DoMove growth and the
    // corresponding UndoMove path still round-trip the full position state.
    TEST_METHOD(DoMoveUndoMoveRoundTripsAfterGameLogGrowth) {
        uint32_t nRng = 0xBADC0DEu;
        auto Rand = [&nRng]() {
            nRng = nRng * 1664525u + 1013904223u;
            return nRng;
        };

        PositionGuard position(CPosition::Initial());
        std::vector<CMove> rgMovesPlayed;
        std::vector<CPosition *> rgSnapshots;

        for (int nPly = 0; nPly < 180; nPly++) {
            PositionGuard snapshot(CPosition::Clone(position.get()));

            heap_t heap = allocate_heap();
            push_section(heap);
            position.get()->LegalMoves(heap);
            std::vector<CMove> rgMoves;
            for (unsigned int i = heap->current_section->start;
                 i < heap->current_section->end; ++i) {
                rgMoves.push_back(heap->data[i]);
            }
            free_heap(heap);
            if (rgMoves.empty()) {
                break;
            }

            CMove move = rgMoves[Rand() % rgMoves.size()];
            position.get()->DoMove(move);
            rgMovesPlayed.push_back(move);
            rgSnapshots.push_back(CPosition::Clone(snapshot.get()));
        }

        for (int i = (int)rgMovesPlayed.size() - 1; i >= 0; i--) {
            position.get()->UndoMove(rgMovesPlayed[i]);
            AssertPositionsEqual(position.get(), rgSnapshots[i]);
            CPosition::Free(rgSnapshots[i]);
        }

        Assert::IsTrue((int)rgMovesPlayed.size() > 128,
                       L"Test must exceed initial game-log size to exercise growth");
    }

    TEST_METHOD(SANRoundTripsForAllLegalMovesInReportedStrategyEPD) {
        const char *pszEpd =
            "1|2/1r|3/3/3|4/4/4/4|4R/5/5/5/5|6/6/6/6/6/4N1|ppppppp/7/7/7/7/2NPN2/PPPQPPP|"
            "r1bq1rk1/p1pp1ppp/1pnbp3/8/8/8/PPPPPPPP/2BQ1B1R|"
            "1nbqb1r/ppppppp/4n2/7/7/PP1PPPP/1NBKB1R|"
            "pppppp/6/6/6/P5/1PPPPP|5/5/5/5/5|4/4/4/4|3/3/3|2/2|1 w - -";
        PositionGuard position(CPosition::CreateFromEPD(pszEpd));
        Assert::IsNotNull(position.get(), L"Failed to create position from EPD");

        heap_t heap = allocate_heap();
        position.get()->LegalMoves(heap);

        for (unsigned int i = heap->current_section->start;
             i < heap->current_section->end; i++) {
            const CMove move = heap->data[i];
            char szSan[32];
            const char *pszSan = position.get()->SAN(move, szSan);
            const CMove parsed = position.get()->ParseSAN(pszSan);

            Assert::IsTrue(parsed != M_NONE,
                           L"Generated SAN should always parse");
            Assert::IsTrue(parsed == move,
                           L"Generated SAN should round-trip to the same legal move");
        }

        free_heap(heap);
    }
};

} // namespace WinAmyTests
