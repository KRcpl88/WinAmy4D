//
// pch.h - precompiled header for the WinAmy console project.
//
// All headers that are expensive to compile and shared across the project are
// gathered here so they are compiled once (into pch.cpp) and reused by every
// translation unit. External (system / standard-library) headers are always
// precompiled; local engine headers are only precompiled when they are large
// or used by most of the project's translation units.
//
#pragma once

// Project-wide compiler defines (must precede any system header pulled in
// below so they take effect for the whole project).
#define WIN32_LEAN_AND_MEAN

// Allow use of unsafe CRT functions (fopen, strerror, etc.).
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// External headers.
#include <string.h>
