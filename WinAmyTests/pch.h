//
// pch.h - precompiled header for the WinAmyTests project.
//
// All headers that are expensive to compile and shared across the project are
// gathered here so they are compiled once (into pch.cpp) and reused by every
// translation unit. Every external (system / standard-library / test-framework)
// header used by the project is precompiled here.
//
// One local header is precompiled: TestHelpers.h. It is included by every test
// translation unit (100% of the .cpp files) and in turn pulls in the core
// engine headers and the shared test helpers, so it is the single project-wide
// local header that belongs in the PCH.
//
#pragma once

// Visual Studio C++ unit-test framework.
#include "CppUnitTest.h"

// C++ standard-library headers.
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cmath>

// Shared test helpers (included by every test translation unit); transitively
// brings in the core engine headers.
#include "TestHelpers.h"
