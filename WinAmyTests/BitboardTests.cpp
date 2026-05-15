#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(BitboardTests) {
  public:
    TEST_METHOD(SetMaskSetsSingleBit) {
        for (int i = 0; i < 64; i++) {
            CBitboard mask(SetMask(i));
            Assert::IsTrue(mask.TstBit(i));
            Assert::AreEqual(1, mask.CountBits());
            Assert::AreEqual(i, mask.FindSetBit());
        }
    }

    TEST_METHOD(ClrMaskClearsSingleBit) {
        for (int i = 0; i < 64; i++) {
            CBitboard mask(ClrMask(i));
            Assert::IsTrue(!mask.TstBit(i));
            Assert::AreEqual(63, mask.CountBits());
            if (i == 0)
                Assert::AreEqual(1, mask.FindSetBit());
            else
                Assert::AreEqual(0, mask.FindSetBit());
        }
    }

    TEST_METHOD(SetBitSetsSpecifiedBit) {
        CBitboard b;
        b.SetBit(0);
        Assert::IsTrue(b.TstBit(0));
        Assert::IsTrue(!b.TstBit(1));
        Assert::IsTrue(!b.TstBit(63));
        Assert::AreEqual(1, b.CountBits());
        Assert::AreEqual(0, b.FindSetBit());

        b.SetBit(63);
        Assert::IsTrue(b.TstBit(0));
        Assert::IsTrue(b.TstBit(63));
        Assert::IsTrue(!b.TstBit(32));
        Assert::AreEqual(2, b.CountBits());
        Assert::AreEqual(0, b.FindSetBit());

        b.SetBit(32);
        Assert::IsTrue(b.TstBit(32));
        Assert::AreEqual(3, b.CountBits());
        Assert::AreEqual(0, b.FindSetBit());

        b.SetBit(0); // setting already-set bit is idempotent
        Assert::IsTrue(b.TstBit(0));
        Assert::AreEqual(3, b.CountBits());
    }

    TEST_METHOD(ClrBitClearsSpecifiedBit) {
        CBitboard b;
        for (int i = 0; i < 64; i++)
            b.SetBit(i);
        Assert::AreEqual(64, b.CountBits());

        b.ClrBit(0);
        Assert::IsTrue(!b.TstBit(0));
        Assert::IsTrue(b.TstBit(1));
        Assert::IsTrue(b.TstBit(63));
        Assert::AreEqual(63, b.CountBits());
        Assert::AreEqual(1, b.FindSetBit());

        b.ClrBit(63);
        Assert::IsTrue(!b.TstBit(63));
        Assert::IsTrue(b.TstBit(32));
        Assert::AreEqual(62, b.CountBits());
        Assert::AreEqual(1, b.FindSetBit());

        b.ClrBit(1);
        Assert::AreEqual(61, b.CountBits());
        Assert::AreEqual(2, b.FindSetBit());

        b.ClrBit(0); // clearing already-clear bit is idempotent
        Assert::IsTrue(!b.TstBit(0));
        Assert::AreEqual(61, b.CountBits());
    }

    TEST_METHOD(TstBitReturnsTrueForSetBits) {
        CBitboard b;
        b.SetBit(e4);
        b.SetBit(a1);
        b.SetBit(h8);
        Assert::IsTrue(b.TstBit(e4));
        Assert::IsTrue(b.TstBit(a1));
        Assert::IsTrue(b.TstBit(h8));
        Assert::IsTrue(!b.TstBit(d4));
        Assert::IsTrue(!b.TstBit(b2));
        Assert::AreEqual(3, b.CountBits());
        Assert::AreEqual((int)a1, b.FindSetBit());
    }

    TEST_METHOD(FindSetBitReturnsLeastSignificantSetBit) {
        CBitboard b;
        b.SetBit(3);
        b.SetBit(5);
        b.SetBit(7);
        Assert::AreEqual(3, b.FindSetBit());

        CBitboard b2;
        b2.SetBit(0);
        Assert::AreEqual(0, b2.FindSetBit());

        CBitboard b3;
        b3.SetBit(63);
        Assert::AreEqual(63, b3.FindSetBit());

        CBitboard b4;
        b4.SetBit(5);
        Assert::AreEqual(5, b4.FindSetBit());

        // All bits set — lowest is 0
        CBitboard b5;
        for (int i = 0; i < 64; i++)
            b5.SetBit(i);
        Assert::AreEqual(0, b5.FindSetBit());
    }

    TEST_METHOD(FindSetBitForEachSquare) {
        for (int i = 0; i < 64; i++) {
            CBitboard bb(SetMask(i));
            Assert::AreEqual(i, bb.FindSetBit());
        }
    }

    TEST_METHOD(FindSetBitAdvancesAsLowBitsCleared) {
        CBitboard b;
        b.SetBit(4);
        b.SetBit(17);
        b.SetBit(33);
        b.SetBit(58);

        Assert::AreEqual(4, b.FindSetBit());
        b.ClrBit(4);
        Assert::AreEqual(17, b.FindSetBit());
        b.ClrBit(17);
        Assert::AreEqual(33, b.FindSetBit());
        b.ClrBit(33);
        Assert::AreEqual(58, b.FindSetBit());
    }

    TEST_METHOD(FindSetBitWithAdjacentBits) {
        for (int i = 0; i < 63; i++) {
            CBitboard b;
            b.SetBit(i);
            b.SetBit(i + 1);
            Assert::AreEqual(i, b.FindSetBit());
        }
    }

    TEST_METHOD(FindSetBitIteratesAllSetBits) {
        CBitboard b;
        int squares[] = {0, 7, 15, 28, 35, 42, 56, 63};
        int numSquares = 8;

        for (int i = 0; i < numSquares; i++)
            b.SetBit(squares[i]);

        Assert::AreEqual(numSquares, b.CountBits());

        for (int i = 0; i < numSquares; i++) {
            int found = b.FindSetBit();
            Assert::AreEqual(squares[i], found);
            b.ClrBit(found);
        }
        Assert::AreEqual(0, b.CountBits());
    }

    TEST_METHOD(CountBitsReturnsNumberOfSetBits) {
        CBitboard empty;
        Assert::AreEqual(0, empty.CountBits());

        CBitboard full;
        for (int i = 0; i < 64; i++)
            full.SetBit(i);
        Assert::AreEqual(64, full.CountBits());

        CBitboard one;
        one.SetBit(0);
        Assert::AreEqual(1, one.CountBits());

        CBitboard oneHigh;
        oneHigh.SetBit(63);
        Assert::AreEqual(1, oneHigh.CountBits());

        CBitboard three;
        three.SetBit(a1);
        three.SetBit(h8);
        three.SetBit(e4);
        Assert::AreEqual(3, three.CountBits());
    }

    TEST_METHOD(CountBitsForPowersOfTwo) {
        for (int i = 0; i < 64; i++) {
            CBitboard bb(SetMask(i));
            Assert::AreEqual(1, bb.CountBits());
        }
    }

    TEST_METHOD(CountBitsForContiguousBits) {
        CBitboard b;
        for (int i = 0; i < 64; i++) {
            b.SetBit(i);
            Assert::AreEqual(i + 1, b.CountBits());
            Assert::AreEqual(0, b.FindSetBit());
        }
    }

    TEST_METHOD(SetBitAndClrBitAreInverses) {
        CBitboard b;
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
            CBitboard combined = CBitboard(SetMask(i)) | CBitboard(ClrMask(i));
            Assert::AreEqual(64, combined.CountBits());
            CBitboard intersection = CBitboard(SetMask(i)) & CBitboard(ClrMask(i));
            Assert::AreEqual(0, intersection.CountBits());
        }
    }
};

} // namespace WinAmyTests
