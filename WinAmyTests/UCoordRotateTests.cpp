#include "TestHelpers.h"

#include "ucoordFloat.h"
#include "ucoordRotate.h"

namespace WinAmyTests {

TEST_CLASS(UCoordRotateTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CUCoordRotate rot;
        Assert::AreEqual(0.0, rot.m_rgAxes[0].getX());
        Assert::AreEqual(0.0, rot.m_rgAxes[1].getX());
        Assert::AreEqual(0.0, rot.m_rgdRotation[0]);
        Assert::AreEqual(0.0, rot.m_rgdRotation[1]);
    }

    TEST_METHOD(ConstructorSetsAxesAndAngles) {
        CUCoordFloat axis0(1.0, 0.0, 0.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate rot(axis0, axis1, 0.5, 1.0);
        Assert::IsTrue(rot.m_rgAxes[0] == axis0);
        Assert::IsTrue(rot.m_rgAxes[1] == axis1);
        Assert::AreEqual(0.5, rot.m_rgdRotation[0]);
        Assert::AreEqual(1.0, rot.m_rgdRotation[1]);
    }

    TEST_METHOD(CopyConstructorCopiesAllFields) {
        CUCoordFloat axis0(1.0, 0.0, 0.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate original(axis0, axis1, 0.3, 0.7);
        CUCoordRotate copy(original);
        Assert::IsTrue(copy.m_rgAxes[0] == original.m_rgAxes[0]);
        Assert::IsTrue(copy.m_rgAxes[1] == original.m_rgAxes[1]);
        Assert::AreEqual(copy.m_rgdRotation[0], original.m_rgdRotation[0]);
        Assert::AreEqual(copy.m_rgdRotation[1], original.m_rgdRotation[1]);
    }

    TEST_METHOD(AssignmentOperatorCopiesAllFields) {
        CUCoordFloat axis0(0.0, 0.0, 1.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate original(axis0, axis1, 1.5, 2.5);
        CUCoordRotate copy;
        copy = original;
        Assert::IsTrue(copy.m_rgAxes[0] == original.m_rgAxes[0]);
        Assert::IsTrue(copy.m_rgAxes[1] == original.m_rgAxes[1]);
        Assert::AreEqual(copy.m_rgdRotation[0], original.m_rgdRotation[0]);
        Assert::AreEqual(copy.m_rgdRotation[1], original.m_rgdRotation[1]);
    }
};

} // namespace WinAmyTests
