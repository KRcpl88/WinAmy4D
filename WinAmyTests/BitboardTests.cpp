#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(BitboardTests) {
  public:
    TEST_METHOD(SetMaskSetsSingleBit) {
        for (int i = 0; i < 64; i++) {
            CBitBoard mask = CBitBoard::SetMask(i);
            Assert::IsTrue(mask.TstBit(i));
            Assert::AreEqual(1, mask.CountBits());
            Assert::AreEqual(i, static_cast<int>(mask.FindSetBit()));
        }
    }

    TEST_METHOD(ClrMaskClearsSingleBit) {
        for (int i = 0; i < 64; i++) {
            CBitBoard mask = CBitBoard::ClrMask(i);
            Assert::IsTrue(!mask.TstBit(i));
            Assert::AreEqual(63, mask.CountBits());
            // FindSetBit should return the lowest set bit, which is 0 unless i==0
            if (i == 0)
                Assert::AreEqual(1, static_cast<int>(mask.FindSetBit()));
            else
                Assert::AreEqual(0, static_cast<int>(mask.FindSetBit()));
        }
    }

    TEST_METHOD(SetBitSetsSpecifiedBit) {
        CBitBoard b = 0;
        b.SetBit(0);
        Assert::IsTrue(b.TstBit(0));
        Assert::IsTrue(!b.TstBit(1));
        Assert::IsTrue(!b.TstBit(63));
        Assert::AreEqual(1, b.CountBits());
        Assert::AreEqual(0, static_cast<int>(b.FindSetBit()));

        b.SetBit(63);
        Assert::IsTrue(b.TstBit(0));
        Assert::IsTrue(b.TstBit(63));
        Assert::IsTrue(!b.TstBit(32));
        Assert::AreEqual(2, b.CountBits());
        Assert::AreEqual(0, static_cast<int>(b.FindSetBit())); // lowest is still bit 0

        b.SetBit(32);
        Assert::IsTrue(b.TstBit(32));
        Assert::AreEqual(3, b.CountBits());
        Assert::AreEqual(0, static_cast<int>(b.FindSetBit()));

        b.SetBit(0); // setting already-set bit is idempotent
        Assert::IsTrue(b.TstBit(0));
        Assert::AreEqual(3, b.CountBits());
    }

    TEST_METHOD(ClrBitClearsSpecifiedBit) {
        CBitBoard b = 0;
        for (int i = 0; i < 64; i++)
            b.SetBit(i);
        Assert::AreEqual(64, b.CountBits());

        b.ClrBit(0);
        Assert::IsTrue(!b.TstBit(0));
        Assert::IsTrue(b.TstBit(1));
        Assert::IsTrue(b.TstBit(63));
        Assert::AreEqual(63, b.CountBits());
        Assert::AreEqual(1, static_cast<int>(b.FindSetBit())); // lowest is now bit 1

        b.ClrBit(63);
        Assert::IsTrue(!b.TstBit(63));
        Assert::IsTrue(b.TstBit(32));
        Assert::AreEqual(62, b.CountBits());
        Assert::AreEqual(1, static_cast<int>(b.FindSetBit())); // lowest still bit 1

        b.ClrBit(1);
        Assert::AreEqual(61, b.CountBits());
        Assert::AreEqual(2, static_cast<int>(b.FindSetBit())); // lowest is now bit 2

        b.ClrBit(0); // clearing already-clear bit is idempotent
        Assert::IsTrue(!b.TstBit(0));
        Assert::AreEqual(61, b.CountBits());
    }

    TEST_METHOD(TstBitReturnsTrueForSetBits) {
        CBitBoard b = 0;
        b.SetBit(e4);
        b.SetBit(a1);
        b.SetBit(h8);
        Assert::IsTrue(b.TstBit(e4));
        Assert::IsTrue(b.TstBit(a1));
        Assert::IsTrue(b.TstBit(h8));
        Assert::IsTrue(!b.TstBit(d4));
        Assert::IsTrue(!b.TstBit(b2));
        Assert::AreEqual(3, b.CountBits());
        Assert::AreEqual((int)a1, static_cast<int>(b.FindSetBit())); // a1 is the lowest square
    }

    TEST_METHOD(FindSetBitReturnsLeastSignificantSetBit) {
        // Build bitboards using SetBit and verify FindSetBit finds the lowest
        CBitBoard b = 0;
        b.SetBit(3);
        b.SetBit(5);
        b.SetBit(7);
        Assert::AreEqual(3, static_cast<int>(b.FindSetBit()));

        CBitBoard b2 = 0;
        b2.SetBit(0);
        Assert::AreEqual(0, static_cast<int>(b2.FindSetBit()));

        CBitBoard b3 = 0;
        b3.SetBit(63);
        Assert::AreEqual(63, static_cast<int>(b3.FindSetBit()));

        CBitBoard b4 = 0;
        b4.SetBit(5);
        Assert::AreEqual(5, static_cast<int>(b4.FindSetBit()));

        // All bits set ΓÇö lowest is 0
        CBitBoard b5 = 0;
        for (int i = 0; i < 64; i++)
            b5.SetBit(i);
        Assert::AreEqual(0, static_cast<int>(b5.FindSetBit()));
    }

    TEST_METHOD(FindSetBitForEachSquare) {
        for (int i = 0; i < 64; i++) {
            Assert::AreEqual(i, static_cast<int>(CBitBoard::SetMask(i).FindSetBit()));
        }
    }

    TEST_METHOD(FindSetBitAdvancesAsLowBitsCleared) {
        // Set bits at various positions, then clear from low to high
        CBitBoard b = 0;
        b.SetBit(4);
        b.SetBit(17);
        b.SetBit(33);
        b.SetBit(58);

        Assert::AreEqual(4, static_cast<int>(b.FindSetBit()));
        b.ClrBit(4);
        Assert::AreEqual(17, static_cast<int>(b.FindSetBit()));
        b.ClrBit(17);
        Assert::AreEqual(33, static_cast<int>(b.FindSetBit()));
        b.ClrBit(33);
        Assert::AreEqual(58, static_cast<int>(b.FindSetBit()));
    }

    TEST_METHOD(FindSetBitWithAdjacentBits) {
        // Two adjacent bits ΓÇö should always find the lower one
        for (int i = 0; i < 63; i++) {
            CBitBoard b = 0;
            b.SetBit(i);
            b.SetBit(i + 1);
            Assert::AreEqual(i, static_cast<int>(b.FindSetBit()));
        }
    }

    TEST_METHOD(FindSetBitIteratesAllSetBits) {
        // Simulate a bit-scan loop: set known bits, extract them one by one
        CBitBoard b = 0;
        int squares[] = {0, 7, 15, 28, 35, 42, 56, 63};
        int numSquares = 8;

        for (int i = 0; i < numSquares; i++)
            b.SetBit(squares[i]);

        Assert::AreEqual(numSquares, b.CountBits());

        // Extract bits in order using FindSetBit + clear pattern
        for (int i = 0; i < numSquares; i++) {
            int found = b.FindSetBit();
            Assert::AreEqual(squares[i], found);
            b.ClrBit(found);
        }
        Assert::AreEqual(0, b.CountBits());
    }

    TEST_METHOD(CountBitsReturnsNumberOfSetBits) {
        CBitBoard empty = 0;
        Assert::AreEqual(0, empty.CountBits());

        CBitBoard full = 0;
        for (int i = 0; i < 64; i++)
            full.SetBit(i);
        Assert::AreEqual(64, full.CountBits());

        CBitBoard one = 0;
        one.SetBit(0);
        Assert::AreEqual(1, one.CountBits());

        CBitBoard oneHigh = 0;
        oneHigh.SetBit(63);
        Assert::AreEqual(1, oneHigh.CountBits());

        CBitBoard three = 0;
        three.SetBit(a1);
        three.SetBit(h8);
        three.SetBit(e4);
        Assert::AreEqual(3, three.CountBits());
    }

    TEST_METHOD(CountBitsForPowersOfTwo) {
        for (int i = 0; i < 64; i++) {
            Assert::AreEqual(1, CBitBoard::SetMask(i).CountBits());
        }
    }

    TEST_METHOD(CountBitsForContiguousBits) {
        CBitBoard b = 0;
        for (int i = 0; i < 64; i++) {
            b.SetBit(i);
            Assert::AreEqual(i + 1, b.CountBits());
            Assert::AreEqual(0, static_cast<int>(b.FindSetBit())); // lowest bit is always 0
        }
    }

    TEST_METHOD(SetBitAndClrBitAreInverses) {
        CBitBoard b = 0;
        for (int i = 0; i < 64; i++) {
            b.SetBit(i);
        }
        Assert::AreEqual(64, b.CountBits());
        for (int i = 0; i < 64; i++) {
            Assert::IsTrue(b.TstBit(i));
        }
        for (int i = 0; i < 64; i++) {
            b.ClrBit(i);
        }
        Assert::AreEqual(0, b.CountBits());
        for (int i = 0; i < 64; i++) {
            Assert::IsTrue(!b.TstBit(i));
        }
    }

    TEST_METHOD(SetMaskAndClrMaskAreComplements) {
        for (int i = 0; i < 64; i++) {
            CBitBoard combined = CBitBoard::SetMask(i) | CBitBoard::ClrMask(i);
            Assert::AreEqual(64, combined.CountBits());
            CBitBoard intersection = CBitBoard::SetMask(i) & CBitBoard::ClrMask(i);
            Assert::AreEqual(0, intersection.CountBits());
        }
    }
};

} // namespace WinAmyTests