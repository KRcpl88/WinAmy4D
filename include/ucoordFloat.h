#pragma once

#include "ucoord.h"

class CUCoordFloat {
public:
    double m_rgdData[3]{};

    CUCoordFloat() = default;
    CUCoordFloat(double x, double y, double z);
    CUCoordFloat(const CUCoordFloat& other) = default;
    explicit CUCoordFloat(const CUCoord& coord);

    double getX() const;
    void setX(double value);
    double getY() const;
    void setY(double value);
    double getZ() const;
    void setZ(double value);

    double& operator[](int index);
    const double& operator[](int index) const;

    CUCoordFloat& operator=(const CUCoordFloat& other) = default;
    bool operator==(const CUCoordFloat& other) const;
    CUCoordFloat operator+(const CUCoordFloat& other) const;
};
