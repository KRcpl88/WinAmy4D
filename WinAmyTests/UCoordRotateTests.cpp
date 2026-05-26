#include "TestHelpers.h"

#include "ucoordFloat.h"
#include "ucoordRotate.h"

namespace WinAmyTests {

TEST_CLASS(UCoordRotateTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CUCoordRotate rot;
        Assert::AreEqual(0.0, rot.GetAxis(0).GetX());
        Assert::AreEqual(0.0, rot.GetAxis(1).GetX());
        Assert::AreEqual(0.0, rot.GetRotation(0));
        Assert::AreEqual(0.0, rot.GetRotation(1));
    }

    TEST_METHOD(ConstructorSetsAxesAndAngles) {
        CUCoordFloat axis0(1.0, 0.0, 0.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate rot(axis0, axis1, 0.5, 1.0);
        Assert::IsTrue(rot.GetAxis(0) == axis0);
        Assert::IsTrue(rot.GetAxis(1) == axis1);
        Assert::AreEqual(0.5, rot.GetRotation(0));
        Assert::AreEqual(1.0, rot.GetRotation(1));
    }

    TEST_METHOD(CopyConstructorCopiesAllFields) {
        CUCoordFloat axis0(1.0, 0.0, 0.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate original(axis0, axis1, 0.3, 0.7);
        CUCoordRotate copy(original);
        Assert::IsTrue(copy.GetAxis(0) == original.GetAxis(0));
        Assert::IsTrue(copy.GetAxis(1) == original.GetAxis(1));
        Assert::AreEqual(copy.GetRotation(0), original.GetRotation(0));
        Assert::AreEqual(copy.GetRotation(1), original.GetRotation(1));
    }

    TEST_METHOD(AssignmentOperatorCopiesAllFields) {
        CUCoordFloat axis0(0.0, 0.0, 1.0);
        CUCoordFloat axis1(0.0, 1.0, 0.0);
        CUCoordRotate original(axis0, axis1, 1.5, 2.5);
        CUCoordRotate copy;
        copy = original;
        Assert::IsTrue(copy.GetAxis(0) == original.GetAxis(0));
        Assert::IsTrue(copy.GetAxis(1) == original.GetAxis(1));
        Assert::AreEqual(copy.GetRotation(0), original.GetRotation(0));
        Assert::AreEqual(copy.GetRotation(1), original.GetRotation(1));
    }

    TEST_METHOD(SetAxisUpdatesValue) {
        CUCoordRotate rot;
        CUCoordFloat axis(1.0, 2.0, 3.0);
        rot.SetAxis(0, axis);
        Assert::IsTrue(rot.GetAxis(0) == axis);
    }

    TEST_METHOD(SetRotationUpdatesValue) {
        CUCoordRotate rot;
        rot.SetRotation(1, 3.14);
        Assert::AreEqual(3.14, rot.GetRotation(1));
    }

    TEST_METHOD(ArrayConstructorSetsAxesAndRotations) {
        CUCoordFloat axes[2] = { CUCoordFloat(1.0, 0.0, 0.0), CUCoordFloat(0.0, 1.0, 0.0) };
        double rotations[2] = { 0.25, 0.75 };
        CUCoordRotate rot(axes, rotations);
        Assert::IsTrue(rot.GetAxis(0) == axes[0]);
        Assert::IsTrue(rot.GetAxis(1) == axes[1]);
        Assert::AreEqual(0.25, rot.GetRotation(0));
        Assert::AreEqual(0.75, rot.GetRotation(1));
    }
};

} // namespace WinAmyTests
