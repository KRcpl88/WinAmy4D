#pragma once

#include "ucoordFloat.h"

class CPoly {
public:
    CUCoordFloat m_rgPoints[3]{};

    CPoly() = default;
    CPoly(const CUCoordFloat& p0, const CUCoordFloat& p1, const CUCoordFloat& p2);
    CPoly(const CPoly& other) = default;

    CPoly& operator=(const CPoly& other) = default;
    bool operator==(const CPoly& other) const;
};
