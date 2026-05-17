#include "TestHelpers.h"

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

        const CMove move = make_move(e2, e4, M_PAWND);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->piece[e4]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e2]);
        Assert::AreEqual((int)Black, (int)position.get()->turn);
        Assert::AreEqual(1, (int)position.get()->ply);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoNullAndUndoNullRestorePositionFields) {
        // Position with en passant available on d6.
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        position.get()->DoNull();
        Assert::AreEqual((int)Black, (int)position.get()->turn);
        Assert::AreEqual(0, (int)position.get()->enPassant);

        position.get()->UndoNull();
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(RecalcAttacksRebuildsAtkSetDerivedData) {
        // Position with a white bishop on d5.
        char epd[] = "4k3/8/8/3B4/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        for (int i = 0; i < 64; i++) {
            position.get()->atkTo[i] = 0;
            position.get()->atkFr[i] = 0;
        }

        position.get()->RecalcAttacks();

        CBitBoard occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        CBitBoard expectedBishopAttacks(bishop_attacks(d5, occupied));

        Assert::IsTrue(position.get()->atkTo[d5] == expectedBishopAttacks);
        Assert::IsTrue((position.get()->atkFr[e6] & CBitBoard::SetMask(d5)).IsNotEmpty());
        Assert::IsTrue((position.get()->atkFr[c4] & CBitBoard::SetMask(d5)).IsNotEmpty());
    }

    TEST_METHOD(DoMoveCaptureRemovesCapturedPiece) {
        // White knight captures black pawn
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(d4, e6, M_CAPTURE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Knight, (int)position.get()->piece[e6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[d4]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveEnPassantCapturesCorrectly) {
        // White pawn on e5, black pawn just moved d7-d5, en passant on d6
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(e5, d6, M_ENPASSANT);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->piece[d6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e5]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[d5]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveShortCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(e1, g1, M_SCASTLE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)King, (int)position.get()->piece[g1]);
        Assert::AreEqual((int)Rook, (int)position.get()->piece[f1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[h1]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveLongCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/R3K3 w Q -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(e1, c1, M_LCASTLE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)King, (int)position.get()->piece[c1]);
        Assert::AreEqual((int)Rook, (int)position.get()->piece[d1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[a1]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMovePromotionChangesType) {
        char epd[] = "7k/4P3/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(e7, e8, static_cast<int>((CMove)Queen << M_PROMOTION_OFFSET));
        position.get()->DoMove(move);

        Assert::AreEqual((int)Queen, (int)position.get()->piece[e8]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e7]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(MFromAndMToDecodeFromScooordBitfields) {
        const CSCoord fromSquare(0, 4, 1); // e2
        const CSCoord toSquare(0, 4, 3);   // e4
        const CMove move = make_move(static_cast<int>(fromSquare), static_cast<int>(toSquare), M_PAWND);

        Assert::AreEqual(static_cast<int>(fromSquare), move.GetFrom());
        Assert::AreEqual(static_cast<int>(toSquare), move.GetTo());
    }
};

} // namespace WinAmyTests
