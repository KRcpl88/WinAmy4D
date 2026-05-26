#pragma once

#include "ucoord.h"

class CUCoordFloat {
public:
    double m_rgdData[3]{};

    CUCoordFloat() = default;
    CUCoordFloat(double x, double y, double z);
    CUCoordFloat(const CUCoordFloat& other) = default;
    explicit CUCoordFloat(const CUCoord& coord);

    double GetX() const;
    void SetX(double dValue);
    double GetY() const;
    void SetY(double dValue);
    double GetZ() const;
    void SetZ(double dValue);

    // Legacy lowercase accessors (kept for backward compatibility)
    double getX() const { return GetX(); }
    void setX(double dValue) { SetX(dValue); }
    double getY() const { return GetY(); }
    void setY(double dValue) { SetY(dValue); }
    double getZ() const { return GetZ(); }
    void setZ(double dValue) { SetZ(dValue); }

    double& operator[](int nIndex);
    const double& operator[](int nIndex) const;

    CUCoordFloat& operator=(const CUCoordFloat& other) = default;
    bool operator==(const CUCoordFloat& other) const;
    CUCoordFloat operator+(const CUCoordFloat& other) const;
    // Cross product
    CUCoordFloat operator*(const CUCoordFloat& other) const;

    double Length() const;
    CUCoordFloat Normalize() const;
    double Dot(const CUCoordFloat& other) const;
    CUCoordFloat Cross(const CUCoordFloat& other) const;
    CUCoordFloat Scale(double dScalar) const;
    CUCoordFloat Rotate(const CUCoordFloat& axisVec, double dAngleRad) const;
    double AngleTo(const CUCoordFloat& other) const;
};
