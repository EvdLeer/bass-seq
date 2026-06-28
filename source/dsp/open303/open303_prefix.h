#pragma once
// Compatibility prefix for the Open303/rosic source.
//
// The original code relies on these standard headers being pulled in
// transitively (as emscripten's libc++ did). GCC's libstdc++ does not, so
// memcpy/memmove/abs/free/rand come up as "not declared". This header is
// force-included for the whole library via CMake, so the upstream source
// does not need to be modified.
#include <cstring>   // memcpy, memmove
#include <cstdlib>   // abs, free, rand, malloc
#include <cmath>     // sin, cos, pow, ... (used throughout rosic)
#include <climits>   // INT_MAX
#include <cfloat>    // DBL_MAX, FLT_MAX
