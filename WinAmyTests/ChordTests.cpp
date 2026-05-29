#include "TestHelpers.h"

#include "ucoordfloat.h"
#include "chord.h"

namespace WinAmyTests {

TEST_CLASS(ChordTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CChord chord;
        Assert::AreEqual(0.0, chord[0].GetX());
        Assert::AreEqual(0.0, chord[1].GetX());
    }

    TEST_METHOD(ConstructorSetsStartAndEnd) {
        CUCoordFloat start(1.0, 2.0, 0.0);
        CUCoordFloat end(3.0, 4.0, 0.0);
        CChord chord(start, end);
        Assert::IsTrue(chord[0] == start);
        Assert::IsTrue(chord[1] == end);
    }

    TEST_METHOD(CopyConstructorCopiesStartAndEnd) {
        CUCoordFloat start(1.0, 2.0, 0.0);
        CUCoordFloat end(5.0, 6.0, 0.0);
        CChord original(start, end);
        CChord copy(original);
        Assert::IsTrue(copy[0] == original[0]);
        Assert::IsTrue(copy[1] == original[1]);
    }

    TEST_METHOD(AssignmentOperatorCopiesStartAndEnd) {
        CUCoordFloat start(1.0, 2.0, 0.0);
        CUCoordFloat end(5.0, 6.0, 0.0);
        CChord original(start, end);
        CChord copy;
        copy = original;
        Assert::IsTrue(copy[0] == original[0]);
        Assert::IsTrue(copy[1] == original[1]);
    }

    TEST_METHOD(IndexOperatorReadsValue) {
        CUCoordFloat start(1.0, 2.0, 0.0);
        CUCoordFloat end(3.0, 4.0, 0.0);
        CChord chord(start, end);
        Assert::IsTrue(chord[0] == start);
        Assert::IsTrue(chord[1] == end);
    }

    TEST_METHOD(IndexOperatorWritesValue) {
        CChord chord;
        CUCoordFloat nNewStart(7.0, 8.0, 0.0);
        chord[0] = nNewStart;
        Assert::IsTrue(chord[0] == nNewStart);
    }

    TEST_METHOD(ConstIndexOperatorReadsValue) {
        CUCoordFloat start(2.0, 3.0, 0.0);
        CUCoordFloat end(5.0, 6.0, 0.0);
        const CChord chord(start, end);
        Assert::IsTrue(chord[0] == start);
        Assert::IsTrue(chord[1] == end);
    }

    TEST_METHOD(EqualityOperatorReturnsTrueForIdentical) {
        CUCoordFloat start(1.0, 2.0, 0.0);
        CUCoordFloat end(3.0, 4.0, 0.0);
        CChord a(start, end);
        CChord b(start, end);
        Assert::IsTrue(a == b);
    }

    TEST_METHOD(EqualityOperatorReturnsFalseForDifferentEnd) {
        CUCoordFloat start(1.0, 2.0, 0.0);
        CChord a(start, CUCoordFloat(3.0, 4.0, 0.0));
        CChord b(start, CUCoordFloat(5.0, 6.0, 0.0));
        Assert::IsFalse(a == b);
    }
};

} // namespace WinAmyTests
