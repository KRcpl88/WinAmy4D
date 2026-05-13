#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(BitboardTests) {
  public:
    TEST_METHOD(SetMaskSetsSingleBit) {
        for (int i = 0; i < 64; i++) {
            BitBoard mask = SetMask(i);
            Assert::IsTrue(TstBit(mask, i) != 0);
            Assert::AreEqual(1, CountBits(mask));
            Assert::AreEqual(i, FindSetBit(mask));
        }
    }

    TEST_METHOD(ClrMaskClearsSingleBit) {
        for (int i = 0; i < 64; i++) {
            BitBoard mask = ClrMask(i);
            Assert::IsTrue(TstBit(mask, i) == 0);
            Assert::AreEqual(63, CountBits(mask));
            // FindSetBit should return the lowest set bit, which is 0 unless i==0
            if (i == 0)
                Assert::AreEqual(1, FindSetBit(mask));
            else
                Assert::AreEqual(0, FindSetBit(mask));
        }
    }

    TEST_METHOD(SetBitSetsSpecifiedBit) {
        BitBoard b = 0;
        SetBit(b, 0);
        Assert::IsTrue(TstBit(b, 0) != 0);
        Assert::IsTrue(TstBit(b, 1) == 0);
        Assert::IsTrue(TstBit(b, 63) == 0);
        Assert::AreEqual(1, CountBits(b));
        Assert::AreEqual(0, FindSetBit(b));

        SetBit(b, 63);
        Assert::IsTrue(TstBit(b, 0) != 0);
        Assert::IsTrue(TstBit(b, 63) != 0);
        Assert::IsTrue(TstBit(b, 32) == 0);
        Assert::AreEqual(2, CountBits(b));
        Assert::AreEqual(0, FindSetBit(b)); // lowest is still bit 0

        SetBit(b, 32);
        Assert::IsTrue(TstBit(b, 32) != 0);
        Assert::AreEqual(3, CountBits(b));
        Assert::AreEqual(0, FindSetBit(b));

        SetBit(b, 0); // setting already-set bit is idempotent
        Assert::IsTrue(TstBit(b, 0) != 0);
        Assert::AreEqual(3, CountBits(b));
    }

    TEST_METHOD(ClrBitClearsSpecifiedBit) {
        BitBoard b = 0;
        for (int i = 0; i < 64; i++)
            SetBit(b, i);
        Assert::AreEqual(64, CountBits(b));

        ClrBit(b, 0);
        Assert::IsTrue(TstBit(b, 0) == 0);
        Assert::IsTrue(TstBit(b, 1) != 0);
        Assert::IsTrue(TstBit(b, 63) != 0);
        Assert::AreEqual(63, CountBits(b));
        Assert::AreEqual(1, FindSetBit(b)); // lowest is now bit 1

        ClrBit(b, 63);
        Assert::IsTrue(TstBit(b, 63) == 0);
        Assert::IsTrue(TstBit(b, 32) != 0);
        Assert::AreEqual(62, CountBits(b));
        Assert::AreEqual(1, FindSetBit(b)); // lowest still bit 1

        ClrBit(b, 1);
        Assert::AreEqual(61, CountBits(b));
        Assert::AreEqual(2, FindSetBit(b)); // lowest is now bit 2

        ClrBit(b, 0); // clearing already-clear bit is idempotent
        Assert::IsTrue(TstBit(b, 0) == 0);
        Assert::AreEqual(61, CountBits(b));
    }

    TEST_METHOD(TstBitReturnsTrueForSetBits) {
        BitBoard b = 0;
        SetBit(b, e4);
        SetBit(b, a1);
        SetBit(b, h8);
        Assert::IsTrue(TstBit(b, e4) != 0);
        Assert::IsTrue(TstBit(b, a1) != 0);
        Assert::IsTrue(TstBit(b, h8) != 0);
        Assert::IsTrue(TstBit(b, d4) == 0);
        Assert::IsTrue(TstBit(b, b2) == 0);
        Assert::AreEqual(3, CountBits(b));
        Assert::AreEqual((int)a1, FindSetBit(b)); // a1 is the lowest square
    }

    TEST_METHOD(FindSetBitReturnsLeastSignificantSetBit) {
        // Build bitboards using SetBit and verify FindSetBit finds the lowest
        BitBoard b = 0;
        SetBit(b, 3);
        SetBit(b, 5);
        SetBit(b, 7);
        Assert::AreEqual(3, FindSetBit(b));

        BitBoard b2 = 0;
        SetBit(b2, 0);
        Assert::AreEqual(0, FindSetBit(b2));

        BitBoard b3 = 0;
        SetBit(b3, 63);
        Assert::AreEqual(63, FindSetBit(b3));

        BitBoard b4 = 0;
        SetBit(b4, 5);
        Assert::AreEqual(5, FindSetBit(b4));

        // All bits set — lowest is 0
        BitBoard b5 = 0;
        for (int i = 0; i < 64; i++)
            SetBit(b5, i);
        Assert::AreEqual(0, FindSetBit(b5));
    }

    TEST_METHOD(FindSetBitForEachSquare) {
        for (int i = 0; i < 64; i++) {
            Assert::AreEqual(i, FindSetBit(SetMask(i)));
        }
    }

    TEST_METHOD(FindSetBitAdvancesAsLowBitsCleared) {
        // Set bits at various positions, then clear from low to high
        BitBoard b = 0;
        SetBit(b, 4);
        SetBit(b, 17);
        SetBit(b, 33);
        SetBit(b, 58);

        Assert::AreEqual(4, FindSetBit(b));
        ClrBit(b, 4);
        Assert::AreEqual(17, FindSetBit(b));
        ClrBit(b, 17);
        Assert::AreEqual(33, FindSetBit(b));
        ClrBit(b, 33);
        Assert::AreEqual(58, FindSetBit(b));
    }

    TEST_METHOD(FindSetBitWithAdjacentBits) {
        // Two adjacent bits — should always find the lower one
        for (int i = 0; i < 63; i++) {
            BitBoard b = 0;
            SetBit(b, i);
            SetBit(b, i + 1);
            Assert::AreEqual(i, FindSetBit(b));
        }
    }

    TEST_METHOD(FindSetBitIteratesAllSetBits) {
        // Simulate a bit-scan loop: set known bits, extract them one by one
        BitBoard b = 0;
        int squares[] = {0, 7, 15, 28, 35, 42, 56, 63};
        int numSquares = 8;

        for (int i = 0; i < numSquares; i++)
            SetBit(b, squares[i]);

        Assert::AreEqual(numSquares, CountBits(b));

        // Extract bits in order using FindSetBit + clear pattern
        for (int i = 0; i < numSquares; i++) {
            int found = FindSetBit(b);
            Assert::AreEqual(squares[i], found);
            ClrBit(b, found);
        }
        Assert::AreEqual(0, CountBits(b));
    }

    TEST_METHOD(CountBitsReturnsNumberOfSetBits) {
        BitBoard empty = 0;
        Assert::AreEqual(0, CountBits(empty));

        BitBoard full = 0;
        for (int i = 0; i < 64; i++)
            SetBit(full, i);
        Assert::AreEqual(64, CountBits(full));

        BitBoard one = 0;
        SetBit(one, 0);
        Assert::AreEqual(1, CountBits(one));

        BitBoard oneHigh = 0;
        SetBit(oneHigh, 63);
        Assert::AreEqual(1, CountBits(oneHigh));

        BitBoard three = 0;
        SetBit(three, a1);
        SetBit(three, h8);
        SetBit(three, e4);
        Assert::AreEqual(3, CountBits(three));
    }

    TEST_METHOD(CountBitsForPowersOfTwo) {
        for (int i = 0; i < 64; i++) {
            Assert::AreEqual(1, CountBits(SetMask(i)));
        }
    }

    TEST_METHOD(CountBitsForContiguousBits) {
        BitBoard b = 0;
        for (int i = 0; i < 64; i++) {
            SetBit(b, i);
            Assert::AreEqual(i + 1, CountBits(b));
            Assert::AreEqual(0, FindSetBit(b)); // lowest bit is always 0
        }
    }

    TEST_METHOD(SetBitAndClrBitAreInverses) {
        BitBoard b = 0;
        for (int i = 0; i < 64; i++) {
            SetBit(b, i);
        }
        Assert::AreEqual(64, CountBits(b));
        for (int i = 0; i < 64; i++) {
            Assert::IsTrue(TstBit(b, i) != 0);
        }
        for (int i = 0; i < 64; i++) {
            ClrBit(b, i);
        }
        Assert::AreEqual(0, CountBits(b));
        for (int i = 0; i < 64; i++) {
            Assert::IsTrue(TstBit(b, i) == 0);
        }
    }

    TEST_METHOD(SetMaskAndClrMaskAreComplements) {
        for (int i = 0; i < 64; i++) {
            BitBoard combined = SetMask(i) | ClrMask(i);
            Assert::AreEqual(64, CountBits(combined));
            BitBoard intersection = SetMask(i) & ClrMask(i);
            Assert::AreEqual(0, CountBits(intersection));
        }
    }
};

} // namespace WinAmyTests
