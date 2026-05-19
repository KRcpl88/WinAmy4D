#pragma once

#include "scoord.h"


class CHCoord {
public:
    static const int Relu16[15];
    static const int NegRelu16[15];

    int m_nLevel{0};
    int m_nRank{0};
    int m_nFile{0};

    CHCoord() = default;
    CHCoord(int level, int file, int rank);
    explicit CHCoord(const CSCoord& scoord);

    bool IsValid() const;

    static bool IsValid(int level, int file, int rank);
    static int RankWidth(int level, int rank);
    static bool IsValid(int offset);

    explicit operator int() const;
    explicit operator CSCoord() const;

private:
    void Validate() const;
};
