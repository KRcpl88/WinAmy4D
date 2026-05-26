#include "TestHelpers.h"

#include "ucoordFloat.h"
#include "poly.h"

namespace WinAmyTests {

TEST_CLASS(PolyTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CPoly poly;
        Assert::AreEqual(0.0, poly.m_rgPoints[0].getX());
        Assert::AreEqual(0.0, poly.m_rgPoints[1].getX());
        Assert::AreEqual(0.0, poly.m_rgPoints[2].getX());
    }

    TEST_METHOD(ConstructorSetsThreePoints) {
        CUCoordFloat p0(1.0, 0.0, 0.0);
        CUCoordFloat p1(0.0, 1.0, 0.0);
        CUCoordFloat p2(0.0, 0.0, 1.0);
        CPoly poly(p0, p1, p2);
        Assert::IsTrue(poly.m_rgPoints[0] == p0);
        Assert::IsTrue(poly.m_rgPoints[1] == p1);
        Assert::IsTrue(poly.m_rgPoints[2] == p2);
    }

    TEST_METHOD(CopyConstructorCopiesAllPoints) {
        CUCoordFloat p0(1.0, 2.0, 3.0);
        CUCoordFloat p1(4.0, 5.0, 6.0);
        CUCoordFloat p2(7.0, 8.0, 9.0);
        CPoly original(p0, p1, p2);
        CPoly copy(original);
        Assert::IsTrue(copy.m_rgPoints[0] == original.m_rgPoints[0]);
        Assert::IsTrue(copy.m_rgPoints[1] == original.m_rgPoints[1]);
        Assert::IsTrue(copy.m_rgPoints[2] == original.m_rgPoints[2]);
    }

    TEST_METHOD(AssignmentOperatorCopiesAllPoints) {
        CUCoordFloat p0(1.0, 2.0, 3.0);
        CUCoordFloat p1(4.0, 5.0, 6.0);
        CUCoordFloat p2(7.0, 8.0, 9.0);
        CPoly original(p0, p1, p2);
        CPoly copy;
        copy = original;
        Assert::IsTrue(copy.m_rgPoints[0] == original.m_rgPoints[0]);
        Assert::IsTrue(copy.m_rgPoints[1] == original.m_rgPoints[1]);
        Assert::IsTrue(copy.m_rgPoints[2] == original.m_rgPoints[2]);
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
