Please refactor Bitboard.c as a C++ class.

1. Rename the file Bitboard.cpp, and the class name will be CBitboard
2. Create class accessor methods for all the bitboard functions in bitboard.h, including SetBit, TestBit, etc.  as well as the functions in botboard.c
3. Add static operator overrides for CBitboard for ==, !=. bitwise &, ~, |.  The operators chould be declared as static and should take too CBitboard objects as operands and return the result as a new CBitboard object.
4. There should be a cast constructor CBitboard (uint64_t )  that will create a CBitboard from a uint64_t bitboard bits.
5. Rename typedef uint64_t BitBoard to BitBoardBits, but otherwise keep that typedef for the bitboard data (which is a uint64_t)
6. Update all unit tests and other code to use CBitboard instead of uint64_t, BitBoard to BitBoardBits when possible.  In some cases that rely on direct bit manipulation, we can continue to use uint64_t directly (or the typedef BitBoardBits), but if there can be a 1 to 1 conversion to use CBitboard and/or the new CBitboard bitwise operators &, |, ~, == and !=, use those.