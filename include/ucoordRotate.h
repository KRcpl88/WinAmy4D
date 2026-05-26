#pragma once

#include "ucoordFloat.h"

class CUCoordRotate {
public:
    CUCoordFloat m_rgAxes[2]{};
    double m_rgdRotation[2]{};

    CUCoordRotate() = default;
    CUCoordRotate(const CUCoordFloat& axis0, const CUCoordFloat& axis1,
                  double rotation0, double rotation1);
    CUCoordRotate(const CUCoordRotate& other) = default;
    CUCoordRotate& operator=(const CUCoordRotate& other) = default;
};
