#pragma once

#include "ucoord.h"

class CUCoordFloat {
    double m_rgdData[3]{};
public:

    CUCoordFloat() = default;
    CUCoordFloat(double dX, double dY, double dZ);
    CUCoordFloat(double rgdData[3]);
    CUCoordFloat(const CUCoordFloat& other) = default;
    explicit CUCoordFloat(const CUCoord& coord);

    double GetX() const;
    void SetX(double dValue);
    double GetY() const;
    void SetY(double dValue);
    double GetZ() const;
    void SetZ(double dValue);

    double& operator[](uint16_t i);
    const double& operator[](uint16_t i) const;

    CUCoordFloat& operator=(const CUCoordFloat& other) = default;
    bool operator==(const CUCoordFloat& other) const;
    CUCoordFloat operator+(const CUCoordFloat& other) const;
};
