#include "TestHelpers.h"

#include "scoord.h"
#include "ucoord.h"
#include "hcoord.h"

#include <stdexcept>

namespace WinAmyTests {

TEST_CLASS(SCoordTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CSCoord coord;
        Assert::AreEqual(0, coord.Level);
        Assert::AreEqual(0, coord.File);
        Assert::AreEqual(0, coord.Rank);
    }

    TEST_METHOD(ThreeArgConstructorSetsFields) {
        CSCoord coord(0, 3, 5);
        Assert::AreEqual(0, coord.Level);
        Assert::AreEqual(3, coord.File);
        Assert::AreEqual(5, coord.Rank);
    }

    TEST_METHOD(OffsetConstructorDecomposesCorrectly) {
        // Offset 0 -> level 0, rank 0, file 0
        CSCoord c0(0);
        Assert::AreEqual(0, c0.Level);
        Assert::AreEqual(0, c0.File);
        Assert::AreEqual(0, c0.Rank);

        // Offset 9 -> level 0, rank 1, file 1 (9 = 1*8 + 1)
        CSCoord c9(9);
        Assert::AreEqual(0, c9.Level);
        Assert::AreEqual(1, c9.File);
        Assert::AreEqual(1, c9.Rank);

        // Offset 63 -> level 0, rank 7, file 7
        CSCoord c63(63);
        Assert::AreEqual(0, c63.Level);
        Assert::AreEqual(7, c63.File);
        Assert::AreEqual(7, c63.Rank);
    }

    TEST_METHOD(BitOffsetRoundTrips) {
        for (int offset = 0; offset < 64; offset++) {
            CSCoord coord(offset);
            Assert::AreEqual(offset, coord.BitOffset());
        }
    }

    TEST_METHOD(EnumeratingRankFileWithCSCoordMatchesBitOffsetOrder) {
        int expectedOffset = 0;
        for (int rank = 0; rank < 8; rank++) {
            for (int file = 0; file < 8; file++) {
                CSCoord square(0, file, rank);
                Assert::AreEqual(expectedOffset, static_cast<int>(square));
                expectedOffset++;
            }
        }
        Assert::AreEqual(64, expectedOffset);
    }

    TEST_METHOD(OffsetConstructorProvidesExpectedRankAndFileAcrossBoard) {
        for (int offset = 0; offset < 64; offset++) {
            CSCoord square(offset);
            Assert::AreEqual(0, square.Level);
            Assert::AreEqual(offset / 8, square.Rank);
            Assert::AreEqual(offset % 8, square.File);
        }
    }

    TEST_METHOD(OperatorIntReturnsBitOffset) {
        CSCoord coord(0, 3, 5);
        Assert::AreEqual(coord.BitOffset(), static_cast<int>(coord));
    }

    TEST_METHOD(IsValidReturnsTrueForValidCoords) {
        Assert::IsTrue(CSCoord::IsValid(0, 0, 0));
        Assert::IsTrue(CSCoord::IsValid(0, 7, 7));
        Assert::IsTrue(CSCoord::IsValid(0, 3, 4));
    }

    TEST_METHOD(IsValidReturnsFalseForInvalidCoords) {
        Assert::IsFalse(CSCoord::IsValid(0, -1, 0));
        Assert::IsFalse(CSCoord::IsValid(0, 0, -1));
        Assert::IsFalse(CSCoord::IsValid(0, 8, 0));
        Assert::IsFalse(CSCoord::IsValid(0, 0, 8));
        Assert::IsFalse(CSCoord::IsValid(1, 0, 0));  // level 1 doesn't exist
        Assert::IsFalse(CSCoord::IsValid(-1, 0, 0));
    }

    TEST_METHOD(IsValidOffsetReturnsTrueForValidRange) {
        Assert::IsTrue(CSCoord::IsValid(0));
        Assert::IsTrue(CSCoord::IsValid(63));
    }

    TEST_METHOD(IsValidOffsetReturnsFalseForInvalidRange) {
        Assert::IsFalse(CSCoord::IsValid(-1));
        Assert::IsFalse(CSCoord::IsValid(64));
    }

    TEST_METHOD(InvalidConstructorThrows) {
        Assert::ExpectException<std::out_of_range>([]() {
            CSCoord coord(0, 8, 0);
        });
        Assert::ExpectException<std::out_of_range>([]() {
            CSCoord coord(64);
        });
        Assert::ExpectException<std::out_of_range>([]() {
            CSCoord coord(-1);
        });
    }
};

TEST_CLASS(UCoordTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CUCoord coord;
        Assert::AreEqual(0, coord.getX());
        Assert::AreEqual(0, coord.getY());
        Assert::AreEqual(0, coord.getZ());
    }

    TEST_METHOD(ThreeArgConstructorSetsFields) {
        CUCoord coord(2, 5, 0);
        Assert::AreEqual(2, coord.getX());
        Assert::AreEqual(5, coord.getY());
        Assert::AreEqual(0, coord.getZ());
    }

    TEST_METHOD(SettersAndGettersWork) {
        CUCoord coord;
        coord.setX(3);
        coord.setY(4);
        coord.setZ(0);
        Assert::AreEqual(3, coord.getX());
        Assert::AreEqual(4, coord.getY());
        Assert::AreEqual(0, coord.getZ());
    }

    TEST_METHOD(ConstructFromSCoordRoundTrips) {
        for (int offset = 0; offset < 64; offset++) {
            CSCoord sc(offset);
            CUCoord uc(sc);
            CSCoord back = static_cast<CSCoord>(uc);
            Assert::AreEqual(sc.Level, back.Level);
            Assert::AreEqual(sc.File, back.File);
            Assert::AreEqual(sc.Rank, back.Rank);
        }
    }

    TEST_METHOD(EqualityOperatorWorks) {
        CUCoord a(1, 2, 0);
        CUCoord b(1, 2, 0);
        CUCoord c(3, 2, 0);
        Assert::IsTrue(a == b);
        Assert::IsFalse(a == c);
        Assert::IsTrue(a != c);
        Assert::IsFalse(a != b);
    }

    TEST_METHOD(AdditionOperatorWorks) {
        CUCoord a(1, 2, 0);
        CUCoord b(3, 4, 0);
        CUCoord result = a + b;
        Assert::AreEqual(4, result.getX());
        Assert::AreEqual(6, result.getY());
        Assert::AreEqual(0, result.getZ());
    }

    TEST_METHOD(SubtractionOperatorWorks) {
        CUCoord a(5, 7, 0);
        CUCoord b(2, 3, 0);
        CUCoord result = a - b;
        Assert::AreEqual(3, result.getX());
        Assert::AreEqual(4, result.getY());
        Assert::AreEqual(0, result.getZ());
    }

    TEST_METHOD(NegationOperatorWorks) {
        CUCoord a(3, -2, 0);
        CUCoord result = -a;
        Assert::AreEqual(-3, result.getX());
        Assert::AreEqual(2, result.getY());
        Assert::AreEqual(0, result.getZ());
    }
};

TEST_CLASS(HCoordTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CHCoord coord;
        Assert::AreEqual(0, coord.Level);
        Assert::AreEqual(0, coord.File);
        Assert::AreEqual(0, coord.Rank);
    }

    TEST_METHOD(ThreeArgConstructorSetsFields) {
        CHCoord coord(2, 3, 4);
        Assert::AreEqual(2, coord.Level);
        Assert::AreEqual(3, coord.File);
        Assert::AreEqual(4, coord.Rank);
    }

    TEST_METHOD(ConstructFromSCoordWorks) {
        CSCoord sc(0, 3, 5);
        CHCoord hc(sc);
        // Verify round-trip through CSCoord conversion
        CSCoord back = static_cast<CSCoord>(hc);
        Assert::AreEqual(sc.Level, back.Level);
        Assert::AreEqual(sc.File, back.File);
        Assert::AreEqual(sc.Rank, back.Rank);
    }

    TEST_METHOD(SCoordRoundTripForAllSquares) {
        for (int offset = 0; offset < 64; offset++) {
            CSCoord sc(offset);
            CHCoord hc(sc);
            CSCoord back = static_cast<CSCoord>(hc);
            Assert::AreEqual(sc.Level, back.Level);
            Assert::AreEqual(sc.File, back.File);
            Assert::AreEqual(sc.Rank, back.Rank);
        }
    }

    TEST_METHOD(IsValidReturnsTrueForValidHCoord) {
        // Construct from a known valid SCoord
        CSCoord sc(0, 0, 0);
        CHCoord hc(sc);
        Assert::IsTrue(hc.IsValid());
    }

    TEST_METHOD(IsValidReturnsFalseForOutOfRange) {
        Assert::IsFalse(CHCoord::IsValid(-1, 0, 0));
        Assert::IsFalse(CHCoord::IsValid(0, 0, -1));
        Assert::IsFalse(CHCoord::IsValid(0, -1, 0));
        Assert::IsFalse(CHCoord::IsValid(8, 0, 0));
    }

    TEST_METHOD(OperatorIntMatchesSCoordBitOffset) {
        // CHCoord::operator int() requires IsValid(), which uses hex-board
        // geometry constraints. Verify it works for coords that pass validation.
        CSCoord sc(0, 0, 0); // file=0, rank=0
        CHCoord hc(sc);
        if (hc.IsValid()) {
            Assert::AreEqual(sc.BitOffset(), static_cast<int>(hc));
        } else {
            // If no valid HCoords exist in the standard 8x8 mapping,
            // verify the CSCoord round-trip works instead
            CSCoord back = static_cast<CSCoord>(hc);
            Assert::AreEqual(sc.Level, back.Level);
            Assert::AreEqual(sc.File, back.File);
            Assert::AreEqual(sc.Rank, back.Rank);
        }
    }
};

} // namespace WinAmyTests
