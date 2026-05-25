Please refactor Bitboard.c as a C++ class.

1. Rename the file Bitboard.cpp, and the class name will be CBitboard
2. Create class accessor methods for all the bitboard functions in bitboard.h, including SetBit, TestBit, etc.  as well as the functions in botboard.c
3. Add static operator overrides for CBitboard for ==, !=. bitwise &, ~, |.  The operators chould be declared as static and should take too CBitboard objects as operands and return the result as a new CBitboard object.
4. There should be a cast constructor CBitboard (uint64_t )  that will create a CBitboard from a uint64_t bitboard bits.
5. Rename typedef uint64_t BitBoard to BitBoardBits, but otherwise keep that typedef for the bitboard data (which is a uint64_t)
6. Update all unit tests and other code to use CBitboard instead of uint64_t, BitBoard to BitBoardBits when possible.  In some cases that rely on direct bit manipulation, we can continue to use uint64_t directly (or the typedef BitBoardBits), but if there can be a 1 to 1 conversion to use CBitboard and/or the new CBitboard bitwise operators &, |, ~, == and !=, use those.


Please refactor Position to C++

1. Refactor the Position struct as a C++ class CPosition
2. Refactor  DoMove, UndoMove, and all other functions which either take struct Position * as a first parameter or create a new Position struct and return a pointer to it, and m,ake sure to remove the old global function versions, they should all be converted to C++ functions
3. IF necessary, also refactor move_t as a C++ class CMove.  But, if the Position struct can be refactored without also refactoring move_t, then we can do that later.
4. Rename CBitboard to CBitBoard
5. Remove the #define for SetMask, ClrMask, SetBit, ClrBit, TstBit, and remove the golbal scope functions CountBits and FindSetBit.  All those typedefs and functions should instead be using the same functions in CBitBoard
6. Update all unit tests and other code to use CPosition instead of Position when possible.


The CSCoord class was designed to convert a chess square position rank and file into a bit offset for the CBitBoard class.

1. The CSCoord Level member is for a multidimensional chessboard, for traditional 2 dimiensional chess on an 8x8 board, it will always be 0.
2. Please update any code which converts a bitboard offset too or from a rank and file to use the SCoord class to do that.
3. Please update any code whihc enumerates the rank and file in a chess board position to enumerate using a CSCoord class using the Rank and File members
4. Please add any new unit tests to verify any logic changes created by using CSCoord instead of directly using level and rank variables inline or locally.


To prepare to expand the logic to a 3D chess board, we need to include level with rank and file.  Because NUM_LEVELS is 1 and LEVEL_WIDTH[0] is 8, these changes will have no affect on the logic.  But, in future commits we will be expanding NUM_LEVELS and LEVEL_WIDTH will be different for each level.

1. In each place where we are enumerating rank and file, add an outer for loop to enumerate level from 0 to NUM_LEVELS
2. each place where we enumerate rank and file form 0 to 7, use LEVEL_WIDTH[level] as the upper limit instead of 8





Please fix all build warnings





Please SAN parsing (including parse_san_with_heap) to parse the level, file and rank for each square instead of just file and rank, such that the SAN notation for square level 1, file a, and rank 1 would be "aa1" and level 1, file g, rank 8 would be "ag8".  In this notation, the move Nac3 would be to move the Knight piece to square "c3" on level 1.  In CSCoord, level, file and rank are 0 based, so that CSCoord(0,0,0) would be level 1 or a, file a, rank 1.  The SAN notation will support levels 1-15, or a - 0



move_t is defined as a 16 bit bitfield with the rank and file of a from and to square.  We need to expand this to accomodate a level, file and rank.  

1. Modify the move_t typedef to be a 32 bit DWORD
2. Define a new typedef scoord_bitfield_t which is a 16 bit WORD bitfield contsining s level, file and rank.  bits 0-3 are the file, bits 4-7 are the rank, and bits 8-11 are the level
3. Add a new CSCoord contructor whihc takes a scoord_bitfield_t and intiialized LEvel, Rank and File from the bitfield
4. Add a new const CSCoord helper function GetBitField which returns a scoord_bitfield_t bitfield
5. Modify M_FROM to construct a CSCoord from the low order 16 bit scoord_bitfield_t of the move_t type, an M_TO to use the high order scoord_bitfield_t





The original C implementation of Amy chess used a 64 bit int bitboard as a bit representation of a chess board, which relied on the specific geometry of the chessboard being exactly 8x8 squares, which is exactly 64 squares and fits in a single 64 bit int as a bitfield. The original implementation also made extensive use of converting a square's file and rank position into a bit offset, and assumed the low order 3 bits of the offset were the file and the high order was the rank. However, the goal of WinAmy4D is to expand this chessboard into 3 dimensions, with many more than the original 64 squares. So all the assumptions of the bit offsets and the rank and file of the chessboard being in certain bit positions in the bit offset are all invalid. This is why the CSCoord class has been introduced, to work with the rank, file, and level position of each square arithmetically instead of making assumptions about the relationship of the rank and file to the bit offsets. In order to fix this, we need to find every place in the code where the original code makes assumptions about the rank and file in the bit offset and instead convert the bit offset into a CSCoord object and then use level, rank and file of the CSCoord instead of making any assumptions about the bit offset. The bit offset should not be used as a location on a chessboard, nor should we be converting back and forth from a bit offset and a CSCoord. The only place a bit offset should be used is internally by CSCoord and CBitBoard to locate the bit offset in the CBitBoard to store a bit for a square, or in some exceptions where we are storing many chess board square locations and for efficiency we want to store them as bit offsets rather than a CSCoord. But, any place we want to check or manipulate the position of a square on a chess board, or in terms of rank and file locations, please use CSCoord. Replace bit offsets with CSCoord, except in a small number of exceptional places where bit offsets are more practical to use.

1. Anyplace we use a bitwise mask operation & 0x7 against a bit offset to obtain the file position of a chess square, use CSCoord instead to convert the bit offset to a CSCoord and compute the rank using CSCoord.File
2. Anyplace we use a bitwise shift operation >> 3 to obtain the rank position of a chess square, use CSCoord instead to convert the bit offset to a CSCoord and compute the rank using CSCoord.Rank
3. Any place we are using a bit offset and making assumptions about which bits in the bit offset encode the rank or file of an actual chess square on the board, use CSCoord instead.
4. Avoid using BitOffset, GetBitOffset, GetFromBitOffset or GetToBitOffset, or converting a CSCoord to or from an int bit offset, if the code is using the bit offset to enumerate, manipulate or compute the file and rank location of a square on a chessboard, convert the code to using a CSCoord to convert the bit offset into a rank and file location of the square.
5. If after these changes, BitOffset, GetBitOffset, GetFromBitOffset or GetToBitOffset are not longer used, remove them from the code.
6. GetBitOffset returns an int, not a int8_t or uint8_t.  Conversion from int to an 8 bit integer type will cause data loss from truncation.  Anyplace we are storing a bit offset as an 8 bit integer type, make sure the integer is at list 16 bits of more.
7. BitOffset and GetBitOffset are redundant functions, remove GetBitOffset and just use BitOffset, if it is still needed, but only if the bit offset is NOT being used to measure the rank or file location of a square on the chess board.
8. In many places we convert a square position to an offset for the purpose of enumerating squares, this is OK as long as we convert back to a CSCoord to compute the rank and file location of the chess square. However, the chessboard square or bit offset must be stored at least as 16 bits, because the final 3D chess board will have 344 squares and will require bit offsets > 255




# Struct renaming

1. Rename all structs to begin with S and use PascalCase, for example SGameLog
2. Rename all class member variables to begin with m_, use  hungarian notation, and PascalCase.   For example int m_nRank.   The correct hungarian notation for a member which is a class or struct  is just an empty notation, for example CSCoord m_From or CSCoord From.  Other common hungarian notations are c for a count of elements, cb for a count of bytes, f for a boolean or flag, bf for a bit field, and rg for an array.   p is a pointer.   p or rg are followed by the type of array or pointer for exampl pn is a pointer to an  int and rgn is an array of ints.  Also w is a word and dw is a double word, usually 16 and 32 bit unsigned ints respectivly.   ul is an insigned long integer and ull is an unsigned long long or 64 bit integer.
3. Rename all local variables with correct hungarian notation



# CSearchData

1. Please convert SearchData into a class CSearchData.  Rename next.h and next.cpp search.h and search.cpp
2. Make all functions in next.h which take a SearchData struct as the first parameter member functions of CSearchData
3. Move the implementation of TestNextGenerators into the CPosition cpp file and make it a member function of CPosition.
4. Create unit tests for all CSearchData member functions.
5. rename elements of the SearchData struct using m_ , add hungarian notation to the member function names, and convert them to PascalCase
6. rename member variables with underscores  like best_score using PascalCase and hungarian m_nBestScore
7. Add a descriptive comment to each member function with a list of the inputs and outputs, what each one does, and an overall description of what the function does


# CPosition
move all non class global or static functions in search.cpp which take a CPosition as a first parameter into the CPosition class and move their implementation with the associated function header comments into the position.cpp file with the rest of the CPosition class implementation


# algorithm

The process of computing an optimal chess move fir each side in WinAmy consists of 3 steps

1. Enumerating all possible moves from the current position
2. Evaluating and scoring each move
3. Choosing the best next move from all possible moves

Create a detailed design document of each of these 3 steps, including which classes and structs are used, which functions are called in which order, and how they work in the code.

Create a report in md format and commit it to the repo

# 64

There are many arrays in the code that are declared using a fixed size of 64 elements.  In almost every case, this is assuming the board size is 8x8 or 64 squares.  Please update this to use CSCoord::SIZE

1. In many places, such as KnightEPM[64] and KingEPM[64], evaluate each case to determine if the size is based on a standard 8x8 64 square chess board size, or if it is 64 for some other reason.  In each case where the array size is 64 because of the 64 square chess board size, instead use CSCoord::SIZE
2. In every other place in the code where the literal constant value  64 is used, evaluate if this is because the standard 64 square chess board is 64 squares, or for some other reason.   If it is refering to the number of squares on a standard chess board, instead use CSCoord::SIZE



# FindSetBitCoord
1. Add a new CBitBoard member function FindSetBitCoord which will call FindSetBit and return a CSCoord for that bit offset.
2. Search the code and any place we are either calling FindSetBit or using the int cast operator to convert a CSCoord to a bit offset, and then convert the bit offset to a CSCoord, use the FindSetBitCoord member function to directly return a CSCoord instead of a bit offset.






# Deprecating magics.cpp

Using the reccomendations provided in "Implementation path for WinAmy4D" in section 1.5 of C:\git\WinAmy4D\doc\Move-Computation-Design.md, update the design of the Piece-move geometry to use CSCoord::Step using the ATTACK_DELTA for each piece in dbase.cpp.  Also use the unit tests in AttackDeltaTests.cpp as a guideline for how to compute moves using the CSCoord::Step function.  Once these changes have been made, the magic tables will no longer be needed or used and the magic tables, blocker masks, and magic constants can then be removed entirely.  Please be sure to add enough unit tests so that they can be removed safely and we can verify the game logic and move computation still works correctly and the game engine can still play correctly.









# EPD Parsing

Please update the EPD parsing to accommodate multiple levels by filling each chess level from the EPD string.   From level 0 to CSCoord::NUM_LEVELS, each level will have CSCoord::LEVEL_WIDTH[level] rows and columns, each row is delimited with \.   After all the row in the first level have been filled, start filling the next level and continue until the end of the EPD string is reached or cscoord::NUM_LEVELS have been filled.  Make sure that the number of columns in each row does not exceed CSCoord::LEVEL_WIDTH[level]


# compiler optimizations

Make all the static const ints in scoord.h unsigned ints and fix any compiler warnings caused by signed unsigned conversions or comparisons , if necessary convert other ints to unsigned ints if necessary to resolve compiler warnings and a signed integer is not needed in that case




# chess square enums
The chess board square location enums a1 through h8 represent squares on the old 2D chess board on level 8.  Because they represent a holdover from the 2D implementation of the chess program, they in theory should no longer be relevant, except in cases where the mechanics are specific to that level. Carefully examine every place in the code where they are still being used and justify why they are still relevant in the 3D version of the game or if not how the code can be upgraded to use a 3D logic.  Then, make a plan to 2 2 things:

1. Either update or remove any code that uses the 2D chess board enum values.  Some places which may still use the old enums are castling, which only occurs on the king's level which is level h, and other king level specific mechanics.
2. IF they are still needed in some places, at least rename them to include the level in the enum name to ha1 - hh8, and add new enums for the top and bottom corners, aa1 and oa1.  Also make sure any locations which reference these square names.




# NegaScout bug
Please investigate a bug in CSearchData::NegaScout.  UndoMove is called from NegaScout.  UndoMove calls AtkSet at line 987, but the piece type tp is set to 0.  This causes AtkSet to call Panic at line 419.  The main problme seems to be that NegaScout is trying to undo a move where the piece is not valid, whihc should not happen.  It is possible the square does nto have  piece set but its not clear how NEgaScout would try to call UndoMove for an invalid square.

Here is the complete call stack:

ucrtbased.dll!00007ff9928a4805() (Unknown Source:0)
ucrtbased.dll!00007ff9928a49a3() (Unknown Source:0)
ucrtbased.dll!00007ff9928bba9d() (Unknown Source:0)
WinAmy.exe!Panic(CPosition * p) Line 324 (c:\github\WinAmy4D\src\dbase.cpp:324)
WinAmy.exe!CPosition::AtkSet(int type, int color, const CSCoord & squareCoord) Line 419 (c:\github\WinAmy4D\src\dbase.cpp:419)
WinAmy.exe!CPosition::UndoMove(CMove move) Line 987 (c:\github\WinAmy4D\src\dbase.cpp:987)
WinAmy.exe!CSearchData::NegaScout(int alpha, int beta, int depth, int node_type) Line 793 (c:\github\WinAmy4D\src\search.cpp:793)
WinAmy.exe!IterateInt(void * x) Line 1055 (c:\github\WinAmy4D\src\search.cpp:1055)
WinAmy.exe!CPosition::Iterate(int * score_ptr, CMove alternate_move, int * alternate_score_ptr) Line 426 (c:\github\WinAmy4D\src\position.cpp:426)
WinAmy.exe!CPosition::SearchRoot() Line 470 (c:\github\WinAmy4D\src\position.cpp:470)
WinAmy.exe!StateMachine() Line 97 (c:\github\WinAmy4D\src\state_machine.cpp:97)
WinAmy.exe!main(int argc, char * * argv) Line 206 (c:\github\WinAmy4D\WinAmy\main.cpp:206)
WinAmy.exe!invoke_main() Line 79 (d:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:79)
WinAmy.exe!__scrt_common_main_seh() Line 288 (d:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:288)
WinAmy.exe!__scrt_common_main() Line 331 (d:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:331)
WinAmy.exe!mainCRTStartup(void * __formal) Line 17 (d:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_main.cpp:17)
kernel32.dll!00007ff9e20a7374() (Unknown Source:0)
ntdll.dll!00007ff9e253cc91() (Unknown Source:0)

This is a list of the moves in the game so far, each is a pair of SAN moves, white first then black.

id2id4
hd7hd5
hd2hd4
hg8hf6
hc2hc4
hd5xhc4
hg1hf3
ib7hd6
hc1hf4
if7he6
hf4xhd6
hd8xhd6
he2he3
hb7hb5
ha2ha4
hc7hc6
hb1hc3
hb8gc6
ib1ic3
ia7fb5
hf3he5
he6gc4
ic3ie4
hd6gd5
he5xif6
ie7xif6
ie4xif6
he8ie7
ic1ig5
ie7if7
ha4xhb5
id7hd7
hb5xhc6
hd7xhc6
hc3ga4
hc6fc4
ga4xfb5
fc4xfb5
id4id5
ie6xid5
if6xhh8
ig7xhh8
hf1xhc4
hc8hg4
hd1ic1
fb5hc6
ha1hc1
he7he6
ic1if4
if7hg8
he1hg1
hf8hd6
hf2hf3
hg4ff3
ie1ic3
gc6hb5
hc4fb3
hc6hd5
if4id2
gc4hb3
hc1hc2
hb5xic3
ib2xic3
hb3ic4
if1ie3
ic4xid2
ie3xhd5
id2xhf1
hd5xhf6
hg7xhf6
hg1xhf1
hh8ig7
hc2hc6
ig7ba1
hc6xic6
ba1xga1
id1gc2
ha8hc8
ig1ib1
ga1aa1
gc2gg2
gd5hd5
fb3fc2
aa1cc1
ic6xjc6
cc1xgc1
gg2gf3
ff3fc6
jc6xjd6
gc1xgb1
gf3gd3
gb1hc1
hf1if1
hd6xhh2
gd3xgd7
hc1hg1
if1hf2
fc6fb5
jd6xhf6
hd5hg5
gd7hd7
hc8hf8
hf6fd4
hf8jd6
ig5ie3
hg5fe5
fd4fb4
fb5jb5
fb4xia6
jd6xjd1
ia6xgb7
hg1xje1
ib1ie1
jd1xie1
ge1xie1Q
je1xie1
ia1xie1
fe5ba1
gb7xga7
jb5xjf1
hg2xjf1Q
ba1hg7
ga7xha7
hg7hg1
hf2ge2
hg1xgg1
hd7he8
hg8if7
jf1xja6
gg1gg4
ge2he2
gg4ig4
ja6xjb6
ig4ie4
he2id1
id5id4
if2if3



# GUI

Create a plan to create a graphical UI for the WinAmy.lib.  Create the GUI program in a new project folder WinAmyGUI and link to the WinAmy.lib to play the game.

The game should have the following minimal features:

1. Create a new game
2. set search depth from 1 to 9.  Defsult should be 3.
3. Display the board position in a GUI format, including all levels, on a single screen.  This may be difficult.  Either display the board levels side by side some way, or use a semi transparent top down to show all levels at once, or perhaps a perspective 3D view that allows the levels to be viewed at an angle so that all the pieces may be seen.
4. Allow 1, 2, or 0 players (self play).  The user may change the number of players at sny point during the game, and if the play is set to self play, a button will allow the user to pause the game to either view the board or change the number of players
5. During self play, the board will show each move made by the computer as they happen.
6. The user msay undo a move if needed.




# GUI bug
I hit an access violoation in WinAmyGUI.exe: "0xC0000005: Access violation reading location 0x0000000000000000"  when I tried to make a move.

WinAmyGUI.exe!ProbeST(unsigned __int64 key, int * score) Line 350 (c:\github\WinAmy4D\src\hashtable.cpp:350)
WinAmyGUI.exe!EvaluatePositionForWhite(const CPosition * p) Line 2133 (c:\github\WinAmy4D\src\evaluation.cpp:2133)
WinAmyGUI.exe!EvaluatePosition(const CPosition * p) Line 2626 (c:\github\WinAmy4D\src\evaluation.cpp:2626)
WinAmyGUI.exe!CSearchData::Quies(int alpha, int beta, int depth) Line 357 (c:\github\WinAmy4D\src\search.cpp:357)
WinAmyGUI.exe!IterateInt(void * x) Line 1062 (c:\github\WinAmy4D\src\search.cpp:1062)
WinAmyGUI.exe!CPosition::Iterate(int * score_ptr, CMove alternate_move, int * alternate_score_ptr) Line 434 (c:\github\WinAmy4D\src\position.cpp:434)
WinAmyGUI.exe!GameController::StartEngineSearch::__l2::<lambda_1>::operator()() Line 85 (c:\github\WinAmy4D\WinAmyGUI\GameController.cpp:85)
WinAmyGUI.exe!std::invoke<`GameController::StartEngineSearch'::`2'::<lambda_1>>(GameController::StartEngineSearch::__l2::<lambda_1> && _Obj) Line 1670 (c:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207\include\type_traits:1670)
WinAmyGUI.exe!std::thread::_Invoke<std::tuple<`GameController::StartEngineSearch'::`2'::<lambda_1>>,0>(void * _RawVals) Line 60 (c:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207\include\thread:60)
ucrtbased.dll!00007ff982d72ec5() (Unknown Source:0)
kernel32.dll!00007ff9e20a7374() (Unknown Source:0)
ntdll.dll!00007ff9e253cc91() (Unknown Source:0)





# GUI features

1. the scroll bars are not working
2. Odd levels should use a different color from even ones
3. Reorder the levels form top to bottom, in 3 rows.  The top row is levels j-o, the middle row is levels g, h, and i, and the botttom row is a-f
4. on ech row, space all thre levels proportional to their widths, instead of using c onctatn width for every level.






# GUI bugs

2. When selecting a piece to move, the legal moves are only showing up for a pawn




# future cleanup:
CPosition piece should be an Enum type PAWN, ROOK, QUEEN, etc. instead of uchar
Rename member variables m_ with correct Hungarian, m_n for an integer type, m_f for Boolean, m_ for a struct or class type like GameLog, CSCoord or CMove
Rename all structs to begine with S, for example SGameLog instead of GameLog.
Add CBitBoard FindSetBitCoord which returns CSCoord












