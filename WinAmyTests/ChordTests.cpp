#include "TestHelpers.h"

#include "ucoord.h"
#include "chord.h"

namespace WinAmyTests {

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

} // namespace WinAmyTests
