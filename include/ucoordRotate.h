#pragma once

#include "ucoordFloat.h"

class CUCoordRotate {
public:
    CUCoordRotate() = default;
    CUCoordRotate(const CUCoordFloat& axis0, const CUCoordFloat& axis1,
                  double rotation0, double rotation1);
    CUCoordRotate(const CUCoordFloat rgAxes[2], const double rgdRotation[2]);
    CUCoordRotate(const CUCoordRotate& other) = default;
    CUCoordRotate& operator=(const CUCoordRotate& other) = default;

    CUCoordFloat& GetAxis(uint16_t i) { return m_rgAxes[i]; }
    const CUCoordFloat& GetAxis(uint16_t i) const { return m_rgAxes[i]; }
    void SetAxis(uint16_t i, const CUCoordFloat& value) { m_rgAxes[i] = value; }

    double GetRotation(uint16_t i) const { return m_rgdRotation[i]; }
    void SetRotation(uint16_t i, double value) { m_rgdRotation[i] = value; }

private:
    CUCoordFloat m_rgAxes[2]{};
    double m_rgdRotation[2]{};
};
