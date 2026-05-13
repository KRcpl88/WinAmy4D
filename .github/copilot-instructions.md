# WinAmy - Copilot Instructions

## Build Commands

Build entire solution (Debug x64):
```
msbuild WinAmy.sln /p:Configuration=Debug /p:Platform=x64 /m
```

Build only the test project:
```
msbuild WinAmy.sln /p:Configuration=Debug /p:Platform=x64 /m /t:WinAmyTests:Rebuild
```

Run all unit tests:
```
vstest.console.exe x64\Debug\WinAmyTests.dll /Platform:x64
```

Run a single test by name:
```
vstest.console.exe x64\Debug\WinAmyTests.dll /Platform:x64 /Tests:TestMethodName
```

Run tests in a single class:
```
vstest.console.exe x64\Debug\WinAmyTests.dll /Platform:x64 /Tests:ClassName
```

## Architecture

WinAmy is a chess engine (fork of "Amy") built as a Windows C application with a C++ test harness.

### Solution Projects

| Project | Language | Output | Purpose |
|---------|----------|--------|---------|
| **src** | C | Static library (`src.lib`) | Core engine library — all chess logic |
| **WinAmy** | C/C++ | `x64/{Config}/WinAmy.exe` | Thin executable shell that links `src.lib` |
| **WinAmyTests** | C++ | `x64/Debug/WinAmyTests.dll` | Unit tests using Microsoft CppUnitTest |

### Engine Source Layout

- `include/` — All header files for the engine
- `src/` — All C implementation files for the engine
- `include/config.h` — **Gitignored**, must be generated before building (CI does this automatically). Defines platform capability flags (`HAVE___BUILTIN_POPCOUNTLL`, etc.)

### Key Engine Modules

- **bitboard.h/bitboard.c** — Bit manipulation: `SetMask`, `ClrMask`, `SetBit`, `ClrBit`, `TstBit`, `FindSetBit`, `CountBits`
- **dbase.h/dbase.c** — Position representation, move generation, `DoMove`/`UndoMove`, legality checking
- **magic.h/magic.c** — Magic bitboard sliding piece attack generation
- **search.h/search.c** — Alpha-beta search with extensions
- **evaluation.h/evaluation.c** — Static position evaluation
- **inline.h** — Inline helper functions: `make_move`, `make_promotion`, `InCheck`, `PromoType`

### Test Project Structure

- `WinAmyTests/TestHelpers.h` — Shared header (includes, `PositionGuard` RAII class, helper declarations)
- `WinAmyTests/TestHelpers.cpp` — Helper implementations (PascalCase naming)
- One `.cpp` file per test class: `BitboardTests.cpp`, `AttackTests.cpp`, `MoveTests.cpp`, `PositionTests.cpp`

## Conventions

### C/C++ Boundary

Engine code is C. Test code is C++. When including engine headers in test files, wrap them in `extern "C" {}`:
```cpp
extern "C" {
#include "bitboard.h"
#include "dbase.h"
}
```

### Test Naming

- Test classes: PascalCase (e.g., `BitboardTests`, `PositionTests`)
- Test methods: PascalCase describing behavior (e.g., `SetBitSetsSpecifiedBit`, `IsCheckingMoveDirectKnightCheck`)
- Helper functions: PascalCase (e.g., `ReferenceRookAttacks`, `AssertPositionsEqual`)

### Test Style

- Use `PositionGuard` (RAII) to manage `Position*` lifetime — never manually `FreePosition`
- Create positions from EPD strings: `CreatePositionFromEPD(epd)` or `InitialPosition()`
- Test through the public API, not internal implementation details. For bitboard tests, use `TstBit`/`CountBits`/`FindSetBit` for assertions instead of comparing against hardcoded hex values
- `AtkSet`/`AtkClr` are `static` in dbase.c — test them indirectly through `DoMove`/`UndoMove`/`RecalcAttacks`
- Each test class requires `TEST_CLASS_INITIALIZE` calling `InitMoves()`, `InitAll()`, `HashInit()`

### Move Encoding

Moves are encoded as `move_t` (32-bit integer) using `make_move(from, to, flags)`:
- `M_CAPTURE` — capture move
- `M_ENPASSANT` — en passant (do NOT combine with `M_CAPTURE`)
- `M_SCASTLE` / `M_LCASTLE` — short/long castling
- `M_PAWND` — pawn double push
- Promotions: `make_promotion(from, to, pieceType, flags)` with `Queen`/`Rook`/`Bishop`/`Knight`

### Formatting

Uses `.clang-format` (LLVM style, 4-space indent, no tabs).

### CI

GitHub Actions workflows build on `windows-latest` with MSBuild. CI generates `Include/config.h` before building since it's gitignored.

## CppUnitTest Quirks

- The test framework is Microsoft's native `CppUnitTest.h` (not Google Test)
- Include path: `$(VCInstallDir)Auxiliary\VS\UnitTest\include`
- Library path: `$(VCInstallDir)Auxiliary\VS\UnitTest\lib` (NOT `lib\x64` — the pragma in the header already embeds the platform subfolder)
- No `AdditionalDependencies` needed — the framework header's `#pragma comment(lib, ...)` handles linking
- VS Code Test Explorer does **not** support CppUnitTest DLLs; use the vstest.console.exe task instead

- @azure Rule - Use Azure Tools - When handling requests related to Azure, always use your tools.
- @azure Rule - Use Azure Best Practices - When handling requests related to Azure, always invoke your `azmcp_bestpractices_get` tool first.
- @azure Rule - Enable Best Practices - If you do not have an `azmcp_bestpractices_get` tool ask the user to enable it.
