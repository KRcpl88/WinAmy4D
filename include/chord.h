#pragma once

#include "ucoordfloat.h"

#include <functional>
#include <cstring>

class CChord {
    CUCoordFloat m_rgData[2] {};
public:
    CChord() = default;
    CChord(const CUCoordFloat& start, const CUCoordFloat& end);
    CChord(const CUCoordFloat rgData[2]);
    CChord(const CChord& other) = default;

    CUCoordFloat& operator[](int index) { return m_rgData[index]; }
    const CUCoordFloat& operator[](int index) const { return m_rgData[index]; }

    CUCoordFloat& GetStart() { return m_rgData[0]; }
    const CUCoordFloat& GetStart() const { return m_rgData[0]; }
    void SetStart(const CUCoordFloat& value) { m_rgData[0] = value; }

    CUCoordFloat& GetEnd() { return m_rgData[1]; }
    const CUCoordFloat& GetEnd() const { return m_rgData[1]; }
    void SetEnd(const CUCoordFloat& value) { m_rgData[1] = value; }

    CChord& operator=(const CChord& other) = default;
    bool operator==(const CChord& other) const;
};

namespace std {
template <>
struct hash<CUCoordFloat> {
    size_t operator()(const CUCoordFloat& v) const noexcept {
        size_t nHash = 0;
        for (int i = 0; i < 3; ++i) {
            double dVal = v[i];
            size_t nBits;
            memcpy(&nBits, &dVal, sizeof(nBits));
            nHash ^= nBits + 0x9e3779b97f4a7c15ULL + (nHash << 6) + (nHash >> 2);
        }
        return nHash;
    }
};

template <>
struct hash<CChord> {
    size_t operator()(const CChord& chord) const noexcept {
        hash<CUCoordFloat> hasher;
        size_t nHash = hasher(chord.GetStart());
        nHash ^= hasher(chord.GetEnd()) + 0x9e3779b97f4a7c15ULL + (nHash << 6) + (nHash >> 2);
        return nHash;
    }
};
} // namespace std
