#pragma once

#include "scoord.h"

class CUCoord {
public:
    int m_rgnData[3]{};

    CUCoord() = default;
    CUCoord(int x, int y, int z);
    explicit CUCoord(const CSCoord& scoord);

    int getX() const;
    void setX(int value);
    int getY() const;
    void setY(int value);
    int getZ() const;
    void setZ(int value);

    explicit operator CSCoord() const;

    bool operator==(const CUCoord& other) const;
    bool operator!=(const CUCoord& other) const;
    CUCoord operator+(const CUCoord& other) const;
    CUCoord operator-(const CUCoord& other) const;
    CUCoord operator-() const;
};

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

    CUCoordFloat& operator=(const CUCoordFloat& other) = default;
    bool operator==(const CUCoordFloat& other) const;
    CUCoordFloat operator+(const CUCoordFloat& other) const;
};

class CUCoordRotate {
public:
    CUCoordFloat m_rgAxes[2]{};
    double m_rgdwRotation[2]{};

    CUCoordRotate() = default;
    CUCoordRotate(const CUCoordFloat& axis0, const CUCoordFloat& axis1,
                  double rotation0, double rotation1);
    CUCoordRotate(const CUCoordRotate& other) = default;
    CUCoordRotate& operator=(const CUCoordRotate& other) = default;
};

class CChord {
public:
    CUCoord m_Start{};
    CUCoord m_End{};

    CChord() = default;
    CChord(const CUCoord& start, const CUCoord& end);
    CChord(const CChord& other) = default;

    CChord& operator=(const CChord& other) = default;
    bool operator==(const CChord& other) const;
};

class CPoly {
public:
    CUCoordFloat m_rfPoints[3]{};

    CPoly() = default;
    CPoly(const CUCoordFloat& p0, const CUCoordFloat& p1, const CUCoordFloat& p2);
    CPoly(const CPoly& other) = default;

    CPoly& operator=(const CPoly& other) = default;
    bool operator==(const CPoly& other) const;
};
