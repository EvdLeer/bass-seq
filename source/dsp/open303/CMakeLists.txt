# Open303 — Robin Schmidt's TB-303 emulation DSP (rosic library).
# Third-party C++ from the bass-seq web project's wasm/open303-src.
# Built as an isolated static library so JUCE's strict warning flags don't
# apply to it. The SIN_COS_TABLE define selects the table-based sin/cos path
# (same as the project's emscripten build).

file(GLOB OPEN303_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(open303 STATIC ${OPEN303_SOURCES})

# SYSTEM: consumers (the plugin) treat these third-party headers as system
# headers, so their warnings don't show up in our build.
target_include_directories(open303 SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(open303 PUBLIC SIN_COS_TABLE)
target_compile_features(open303 PUBLIC cxx_std_17)

# Required to link into the shared plugin modules (VST3/LV2 .so).
set_target_properties(open303 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Third-party code: silence its warnings, keep our own build output clean.
# Force-include a compatibility prefix so missing <cstring>/<cstdlib> includes
# in the original source don't break the build (see open303_prefix.h).
set(OPEN303_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/open303_prefix.h")
if (MSVC)
    target_compile_options(open303 PRIVATE /w "/FI${OPEN303_PREFIX}")
else()
    target_compile_options(open303 PRIVATE -w -include "${OPEN303_PREFIX}")
endif()
