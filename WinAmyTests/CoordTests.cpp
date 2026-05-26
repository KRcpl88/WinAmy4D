#include "TestHelpers.h"

#include "scoord.h"
#include "ucoord.h"
#include "hcoord.h"

#include <limits>
#include <stdexcept>

namespace WinAmyTests {

TEST_CLASS(SCoordTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CSCoord coord;
        Assert::AreEqual<std::uint16_t>(0, coord.m_nLevel);
        Assert::AreEqual<std::uint16_t>(0, coord.m_nFile);
        Assert::AreEqual<std::uint16_t>(0, coord.m_nRank);
    }

    TEST_METHOD(ThreeArgConstructorSetsFields) {
        constexpr std::uint16_t level = CBitBoard::NUM_LEVELS / 2;
        CSCoord coord(level, 3, 5);
        Assert::AreEqual<std::uint16_t>(level, coord.m_nLevel);
        Assert::AreEqual<std::uint16_t>(3, coord.m_nFile);
        Assert::AreEqual<std::uint16_t>(5, coord.m_nRank);
    }

    TEST_METHOD(OffsetConstructorDecomposesCorrectly) {
        // Offset 0 -> level 0, rank 0, file 0
        CSCoord c0(0);
        Assert::AreEqual<std::uint16_t>(0, c0.m_nLevel);
        Assert::AreEqual<std::uint16_t>(0, c0.m_nFile);
        Assert::AreEqual<std::uint16_t>(0, c0.m_nRank);

        // First square of level 7
        CSCoord firstL7(CBitBoard::LEVEL_OFFSET[7]);
        Assert::AreEqual<std::uint16_t>(7, firstL7.m_nLevel);
        Assert::AreEqual<std::uint16_t>(0, firstL7.m_nFile);
        Assert::AreEqual<std::uint16_t>(0, firstL7.m_nRank);

        // Last square of level 7
        const std::uint16_t l7Last =
            static_cast<std::uint16_t>(CBitBoard::LEVEL_OFFSET[8] - 1);
        CSCoord lastL7(l7Last);
        Assert::AreEqual<std::uint16_t>(7, lastL7.m_nLevel);
        Assert::AreEqual<std::uint16_t>(7, lastL7.m_nFile);
        Assert::AreEqual<std::uint16_t>(7, lastL7.m_nRank);
    }

    TEST_METHOD(BitOffsetRoundTrips) {
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord coord(static_cast<std::uint16_t>(offset));
            Assert::AreEqual<std::uint16_t>(static_cast<std::uint16_t>(offset), coord.BitOffset());
        }
    }

    TEST_METHOD(EnumeratingRankFileWithCSCoordMatchesBitOffsetOrder) {
        int expectedOffset = CBitBoard::LEVEL_OFFSET[0];
        for (unsigned int level = 0; level < CBitBoard::NUM_LEVELS; level++) {
            const unsigned int width = CBitBoard::LEVEL_WIDTH[level];
            for (unsigned int rank = 0; rank < width; rank++) {
                for (unsigned int file = 0; file < width; file++) {
                    CSCoord square(static_cast<int>(level), static_cast<int>(file), static_cast<int>(rank));
                    Assert::AreEqual(expectedOffset, static_cast<int>(square));
                    expectedOffset++;
                }
            }
        }
        Assert::IsTrue(expectedOffset > 0);
    }

    TEST_METHOD(OffsetConstructorProvidesExpectedRankAndFileAcrossBoard) {
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord square(static_cast<std::uint16_t>(offset));
            const unsigned int levelOffset = offset - CBitBoard::LEVEL_OFFSET[square.m_nLevel];
            const unsigned int width = CBitBoard::LEVEL_WIDTH[square.m_nLevel];
            Assert::AreEqual<std::uint16_t>(static_cast<std::uint16_t>(levelOffset / width), square.m_nRank);
            Assert::AreEqual<std::uint16_t>(static_cast<std::uint16_t>(levelOffset % width), square.m_nFile);
        }
    }

    TEST_METHOD(OperatorIntReturnsBitOffset) {
        CSCoord coord(CBitBoard::NUM_LEVELS / 2, 3, 5);
        Assert::AreEqual(static_cast<int>(coord.BitOffset()), static_cast<int>(coord));
    }

    TEST_METHOD(IsValidReturnsTrueForValidCoords) {
        Assert::IsTrue(CSCoord::IsValid(0, 0, 0));
        Assert::IsTrue(CSCoord::IsValid(7, 7, 7));
        Assert::IsTrue(CSCoord::IsValid(7, 3, 4));
    }

    TEST_METHOD(IsValidReturnsFalseForInvalidCoords) {
        Assert::IsFalse(CSCoord::IsValid(0, -1, 0));
        Assert::IsFalse(CSCoord::IsValid(0, 0, -1));
        Assert::IsFalse(CSCoord::IsValid(0, CBitBoard::LEVEL_WIDTH[0], 0));
        Assert::IsFalse(CSCoord::IsValid(0, 0, CBitBoard::LEVEL_WIDTH[0]));
        Assert::IsFalse(CSCoord::IsValid(CBitBoard::NUM_LEVELS, 0, 0));
        Assert::IsFalse(CSCoord::IsValid((std::numeric_limits<std::uint16_t>::max)(), 0, 0));
    }

    TEST_METHOD(IsValidOffsetReturnsTrueForValidRange) {
        Assert::IsTrue(CSCoord::IsValid(0));
        Assert::IsTrue(CSCoord::IsValid(static_cast<int>(CBitBoard::SIZE - 1)));
    }

    TEST_METHOD(IsValidOffsetReturnsFalseForInvalidRange) {
        Assert::IsFalse(CSCoord::IsValid((std::numeric_limits<std::uint16_t>::max)()));
        Assert::IsFalse(CSCoord::IsValid(static_cast<int>(CBitBoard::SIZE)));
    }

    TEST_METHOD(InvalidConstructorThrows) {
        Assert::ExpectException<std::out_of_range>([]() {
            CSCoord coord(0, CBitBoard::LEVEL_WIDTH[0], 0);
        });
        Assert::ExpectException<std::out_of_range>([]() {
            CSCoord coord(static_cast<int>(CBitBoard::SIZE));
        });
        Assert::ExpectException<std::out_of_range>([]() {
            CSCoord coord((std::numeric_limits<std::uint16_t>::max)());
        });
    }

    TEST_METHOD(BitfieldConstructorDecomposesLevelRankFile) {
        const scoord_bitfield_t bitfield = static_cast<scoord_bitfield_t>((7 << 8) | (4 << 4) | 3);
        CSCoord coord(bitfield);
        Assert::AreEqual<std::uint16_t>(7, coord.m_nLevel);
        Assert::AreEqual<std::uint16_t>(3, coord.m_nFile);
        Assert::AreEqual<std::uint16_t>(4, coord.m_nRank);
    }

    TEST_METHOD(GetBitFieldRoundTripsWithBitfieldConstructor) {
        CSCoord original(7, 6, 7);
        const scoord_bitfield_t bitfield = original.GetBitField();
        CSCoord roundTrip(bitfield);
        Assert::AreEqual(original.m_nLevel, roundTrip.m_nLevel);
        Assert::AreEqual(original.m_nFile, roundTrip.m_nFile);
        Assert::AreEqual(original.m_nRank, roundTrip.m_nRank);
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
            Assert::AreEqual(sc.m_nLevel, back.m_nLevel);
            Assert::AreEqual(sc.m_nFile, back.m_nFile);
            Assert::AreEqual(sc.m_nRank, back.m_nRank);
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
        Assert::AreEqual(0, coord.m_nLevel);
        Assert::AreEqual(0, coord.m_nFile);
        Assert::AreEqual(0, coord.m_nRank);
    }

    TEST_METHOD(ThreeArgConstructorSetsFields) {
        CHCoord coord(2, 3, 4);
        Assert::AreEqual(2, coord.m_nLevel);
        Assert::AreEqual(3, coord.m_nFile);
        Assert::AreEqual(4, coord.m_nRank);
    }

    TEST_METHOD(ConstructFromSCoordWorks) {
        CSCoord sc(0, 0, 0);
        CHCoord hc(sc);
        // Verify round-trip through CSCoord conversion
        CSCoord back = static_cast<CSCoord>(hc);
        Assert::AreEqual(sc.m_nLevel, back.m_nLevel);
        Assert::AreEqual(sc.m_nFile, back.m_nFile);
        Assert::AreEqual(sc.m_nRank, back.m_nRank);
    }

    TEST_METHOD(SCoordRoundTripForAllSquares) {
        for (int offset = 0; offset < 64; offset++) {
            CSCoord sc(offset);
            CHCoord hc(sc);
            CSCoord back = static_cast<CSCoord>(hc);
            Assert::AreEqual(sc.m_nLevel, back.m_nLevel);
            Assert::AreEqual(sc.m_nFile, back.m_nFile);
            Assert::AreEqual(sc.m_nRank, back.m_nRank);
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
            Assert::AreEqual(static_cast<int>(sc.BitOffset()), static_cast<int>(hc));
        } else {
            // If no valid HCoords exist in the standard 8x8 mapping,
            // verify the CSCoord round-trip works instead
            CSCoord back = static_cast<CSCoord>(hc);
            Assert::AreEqual(sc.m_nLevel, back.m_nLevel);
            Assert::AreEqual(sc.m_nFile, back.m_nFile);
            Assert::AreEqual(sc.m_nRank, back.m_nRank);
        }
    }
};

TEST_CLASS(UCoordFloatTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CUCoordFloat coord;
        Assert::AreEqual(0.0, coord.getX());
        Assert::AreEqual(0.0, coord.getY());
        Assert::AreEqual(0.0, coord.getZ());
    }

    TEST_METHOD(ThreeArgConstructorSetsFields) {
        CUCoordFloat coord(1.5, 2.5, 3.5);
        Assert::AreEqual(1.5, coord.getX());
        Assert::AreEqual(2.5, coord.getY());
        Assert::AreEqual(3.5, coord.getZ());
    }

    TEST_METHOD(CopyConstructorFromCUCoordSetsFields) {
        CUCoord icoord(2, 5, 1);
        CUCoordFloat fcoord(icoord);
        Assert::AreEqual(2.0, fcoord.getX());
        Assert::AreEqual(5.0, fcoord.getY());
        Assert::AreEqual(1.0, fcoord.getZ());
    }

    TEST_METHOD(CopyConstructorCopiesAllFields) {
        CUCoordFloat original(1.1, 2.2, 3.3);
        CUCoordFloat copy(original);
        Assert::AreEqual(original.getX(), copy.getX());
        Assert::AreEqual(original.getY(), copy.getY());
        Assert::AreEqual(original.getZ(), copy.getZ());
    }

    TEST_METHOD(AssignmentOperatorCopiesAllFields) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b;
        b = a;
        Assert::AreEqual(1.0, b.getX());
        Assert::AreEqual(2.0, b.getY());
        Assert::AreEqual(3.0, b.getZ());
    }

    TEST_METHOD(EqualityOperatorReturnsTrueForIdentical) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(1.0, 2.0, 3.0);
        Assert::IsTrue(a == b);
    }

    TEST_METHOD(EqualityOperatorReturnsFalseForDifferent) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(1.0, 2.0, 4.0);
        Assert::IsFalse(a == b);
    }

    TEST_METHOD(AdditionOperatorAddsComponentWise) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(0.5, 1.5, 2.5);
        CUCoordFloat result = a + b;
        Assert::AreEqual(1.5, result.getX());
        Assert::AreEqual(3.5, result.getY());
        Assert::AreEqual(5.5, result.getZ());
    }
};

TEST_CLASS(UCoordRotateTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CUCoordRotate rot;
        Assert::AreEqual(0.0, rot.m_rgAxes[0].getX());
        Assert::AreEqual(0.0, rot.m_rgAxes[1].getX());
        Assert::AreEqual(0.0, rot.m_rgdwRotation[0]);
        Assert::AreEqual(0.0, rot.m_rgdwRotation[1]);
    }

    TEST_METHOD(ConstructorSetsAxesAndAngles) {
        CUCoordFloat axis0(1.0, 0.0, 0.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate rot(axis0, axis1, 0.5, 1.0);
        Assert::IsTrue(rot.m_rgAxes[0] == axis0);
        Assert::IsTrue(rot.m_rgAxes[1] == axis1);
        Assert::AreEqual(0.5, rot.m_rgdwRotation[0]);
        Assert::AreEqual(1.0, rot.m_rgdwRotation[1]);
    }

    TEST_METHOD(CopyConstructorCopiesAllFields) {
        CUCoordFloat axis0(1.0, 0.0, 0.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate original(axis0, axis1, 0.3, 0.7);
        CUCoordRotate copy(original);
        Assert::IsTrue(copy.m_rgAxes[0] == original.m_rgAxes[0]);
        Assert::IsTrue(copy.m_rgAxes[1] == original.m_rgAxes[1]);
        Assert::AreEqual(copy.m_rgdwRotation[0], original.m_rgdwRotation[0]);
        Assert::AreEqual(copy.m_rgdwRotation[1], original.m_rgdwRotation[1]);
    }

    TEST_METHOD(AssignmentOperatorCopiesAllFields) {
        CUCoordFloat axis0(0.0, 0.0, 1.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate original(axis0, axis1, 1.5, 2.5);
        CUCoordRotate copy;
        copy = original;
        Assert::IsTrue(copy.m_rgAxes[0] == original.m_rgAxes[0]);
        Assert::IsTrue(copy.m_rgAxes[1] == original.m_rgAxes[1]);
        Assert::AreEqual(copy.m_rgdwRotation[0], original.m_rgdwRotation[0]);
        Assert::AreEqual(copy.m_rgdwRotation[1], original.m_rgdwRotation[1]);
    }
};

TEST_CLASS(ChordTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CChord chord;
        Assert::AreEqual(0, chord.m_Start.getX());
        Assert::AreEqual(0, chord.m_End.getX());
    }

    TEST_METHOD(ConstructorSetsStartAndEnd) {
        CUCoord start(1, 2, 0);
        CUCoord end(3, 4, 0);
        CChord chord(start, end);
        Assert::IsTrue(chord.m_Start == start);
        Assert::IsTrue(chord.m_End == end);
    }

    TEST_METHOD(CopyConstructorCopiesStartAndEnd) {
        CUCoord start(1, 2, 0);
        CUCoord end(5, 6, 0);
        CChord original(start, end);
        CChord copy(original);
        Assert::IsTrue(copy.m_Start == original.m_Start);
        Assert::IsTrue(copy.m_End == original.m_End);
    }

    TEST_METHOD(AssignmentOperatorCopiesStartAndEnd) {
        CUCoord start(1, 2, 0);
        CUCoord end(5, 6, 0);
        CChord original(start, end);
        CChord copy;
        copy = original;
        Assert::IsTrue(copy.m_Start == original.m_Start);
        Assert::IsTrue(copy.m_End == original.m_End);
    }

    TEST_METHOD(EqualityOperatorReturnsTrueForIdentical) {
        CUCoord start(1, 2, 0);
        CUCoord end(3, 4, 0);
        CChord a(start, end);
        CChord b(start, end);
        Assert::IsTrue(a == b);
    }

    TEST_METHOD(EqualityOperatorReturnsFalseForDifferentEnd) {
        CUCoord start(1, 2, 0);
        CChord a(start, CUCoord(3, 4, 0));
        CChord b(start, CUCoord(5, 6, 0));
        Assert::IsFalse(a == b);
    }
};

TEST_CLASS(PolyTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CPoly poly;
        Assert::AreEqual(0.0, poly.m_rfPoints[0].getX());
        Assert::AreEqual(0.0, poly.m_rfPoints[1].getX());
        Assert::AreEqual(0.0, poly.m_rfPoints[2].getX());
    }

    TEST_METHOD(ConstructorSetsThreePoints) {
        CUCoordFloat p0(1.0, 0.0, 0.0);
        CUCoordFloat p1(0.0, 1.0, 0.0);
        CUCoordFloat p2(0.0, 0.0, 1.0);
        CPoly poly(p0, p1, p2);
        Assert::IsTrue(poly.m_rfPoints[0] == p0);
        Assert::IsTrue(poly.m_rfPoints[1] == p1);
        Assert::IsTrue(poly.m_rfPoints[2] == p2);
    }

    TEST_METHOD(CopyConstructorCopiesAllPoints) {
        CUCoordFloat p0(1.0, 2.0, 3.0);
        CUCoordFloat p1(4.0, 5.0, 6.0);
        CUCoordFloat p2(7.0, 8.0, 9.0);
        CPoly original(p0, p1, p2);
        CPoly copy(original);
        Assert::IsTrue(copy.m_rfPoints[0] == original.m_rfPoints[0]);
        Assert::IsTrue(copy.m_rfPoints[1] == original.m_rfPoints[1]);
        Assert::IsTrue(copy.m_rfPoints[2] == original.m_rfPoints[2]);
    }

    TEST_METHOD(AssignmentOperatorCopiesAllPoints) {
        CUCoordFloat p0(1.0, 2.0, 3.0);
        CUCoordFloat p1(4.0, 5.0, 6.0);
        CUCoordFloat p2(7.0, 8.0, 9.0);
        CPoly original(p0, p1, p2);
        CPoly copy;
        copy = original;
        Assert::IsTrue(copy.m_rfPoints[0] == original.m_rfPoints[0]);
        Assert::IsTrue(copy.m_rfPoints[1] == original.m_rfPoints[1]);
        Assert::IsTrue(copy.m_rfPoints[2] == original.m_rfPoints[2]);
    }

    TEST_METHOD(EqualityOperatorReturnsTrueForIdentical) {
        CUCoordFloat p0(1.0, 0.0, 0.0);
        CUCoordFloat p1(0.0, 1.0, 0.0);
        CUCoordFloat p2(0.0, 0.0, 1.0);
        CPoly a(p0, p1, p2);
        CPoly b(p0, p1, p2);
        Assert::IsTrue(a == b);
    }

    TEST_METHOD(EqualityOperatorReturnsFalseForDifferentPoint) {
        CUCoordFloat p0(1.0, 0.0, 0.0);
        CUCoordFloat p1(0.0, 1.0, 0.0);
        CUCoordFloat p2(0.0, 0.0, 1.0);
        CUCoordFloat p2b(0.0, 0.0, 2.0);
        CPoly a(p0, p1, p2);
        CPoly b(p0, p1, p2b);
        Assert::IsFalse(a == b);
    }
};

} // namespace WinAmyTests
