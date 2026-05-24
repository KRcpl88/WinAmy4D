#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(CheckmateTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    // Regression test: after 1.id4 Nhc6 2.hd4 he6 3.hc4 Bhb4, white is in
    // check but NOT in checkmate. White can block with Nb1-c3 (and other
    // moves), so LegalMoves must return > 0.
    TEST_METHOD(AfterBishopCheckWhiteHasLegalBlockingMoves) {
        PositionGuard position(CPosition::Initial());

        const char *moves[] = {"id4", "Nhc6", "hd4", "he6", "hc4", "Bhb4"};
        for (const char *san : moves) {
            CMove move = position.get()->ParseSAN(san);
            Assert::IsTrue(move != M_NONE,
                           L"ParseSAN returned M_NONE for move");
            position.get()->DoMove(move);
        }

        // After 3...Bhb4, it is White's turn. The position should be check.
        Assert::IsTrue(position.get()->InCheck(White),
                       L"Expected white king to be in check after Bhb4");

        // White must have at least one legal move (e.g. Nb1-c3 blocking).
        int legalCount = position.get()->LegalMoves(NULL);
        Assert::IsTrue(legalCount > 0,
                       L"Expected white to have legal moves, but got 0 (false checkmate)");
    }

    // Verify specifically that Nb1-c3 is a legal blocking move.
    TEST_METHOD(Nb1c3IsLegalBlockingMoveAfterBishopCheck) {
        PositionGuard position(CPosition::Initial());

        const char *moves[] = {"id4", "Nhc6", "hd4", "he6", "hc4", "Bhb4"};
        for (const char *san : moves) {
            CMove move = position.get()->ParseSAN(san);
            Assert::IsTrue(move != M_NONE,
                           L"ParseSAN returned M_NONE for move");
            position.get()->DoMove(move);
        }

        CMove block = position.get()->ParseSAN("Nhc3");
        Assert::IsTrue(block != M_NONE, L"ParseSAN could not find Nb1-c3");
        Assert::IsTrue(position.get()->LegalMove(block),
                       L"Nb1-c3 should be a legal blocking move");
    }
};

} // namespace WinAmyTests
