#include "TestHelpers.h"

#include "ucoord.h"
#include "chord.h"

namespace WinAmyTests {

TEST_CLASS(ChordTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CChord chord;
        Assert::AreEqual(0, chord[0].GetX());
        Assert::AreEqual(0, chord[1].GetX());
    }

    TEST_METHOD(ConstructorSetsStartAndEnd) {
        CUCoord start(1, 2, 0);
        CUCoord end(3, 4, 0);
        CChord chord(start, end);
        Assert::IsTrue(chord[0] == start);
        Assert::IsTrue(chord[1] == end);
    }

    TEST_METHOD(CopyConstructorCopiesStartAndEnd) {
        CUCoord start(1, 2, 0);
        CUCoord end(5, 6, 0);
        CChord original(start, end);
        CChord copy(original);
        Assert::IsTrue(copy[0] == original[0]);
        Assert::IsTrue(copy[1] == original[1]);
    }

    TEST_METHOD(AssignmentOperatorCopiesStartAndEnd) {
        CUCoord start(1, 2, 0);
        CUCoord end(5, 6, 0);
        CChord original(start, end);
        CChord copy;
        copy = original;
        Assert::IsTrue(copy[0] == original[0]);
        Assert::IsTrue(copy[1] == original[1]);
    }

    TEST_METHOD(IndexOperatorReadsValue) {
        CUCoord start(1, 2, 0);
        CUCoord end(3, 4, 0);
        CChord chord(start, end);
        Assert::IsTrue(chord[0] == start);
        Assert::IsTrue(chord[1] == end);
    }

    TEST_METHOD(IndexOperatorWritesValue) {
        CChord chord;
        CUCoord newStart(7, 8, 0);
        chord[0] = newStart;
        Assert::IsTrue(chord[0] == newStart);
    }

    TEST_METHOD(ConstIndexOperatorReadsValue) {
        CUCoord start(2, 3, 0);
        CUCoord end(5, 6, 0);
        const CChord chord(start, end);
        Assert::IsTrue(chord[0] == start);
        Assert::IsTrue(chord[1] == end);
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

} // namespace WinAmyTests
