//
// pch.h - precompiled header for the WinAmyGUI project.
//
// All headers that are expensive to compile and shared across the project are
// gathered here so they are compiled once (into pch.cpp) and reused by every
// translation unit. Every external (system / standard-library) header used by
// the project is precompiled here.
//
// No local project header is precompiled: none is included by more than half
// of the project's translation units, and the project headers are small, so
// precompiling them would add coupling without a meaningful build-time win.
//
#pragma once

// Project-wide compiler defines (must precede <windows.h> so they take effect
// for the whole project). NOMINMAX suppresses the windows.h min/max macros so
// the project can use std::min / std::max consistently (D3DBoardRenderer.cpp
// and GameController.cpp rely on this).
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Windows / Win32 UI headers.
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wchar.h>

// Direct3D / DirectXMath headers.
#include <d3dcompiler.h>
#include <DirectXMath.h>

// C runtime headers.
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// C++ standard-library headers.
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
