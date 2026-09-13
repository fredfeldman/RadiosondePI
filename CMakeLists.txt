cmake_minimum_required(VERSION 3.20)
project(RadiosondePI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization & Compiler Flags
if(MSVC)
    add_compile_options(/EHsc /utf-8 /Zc:__cplusplus /permissive-)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
        add_compile_options(/O2 /fp:fast)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
        add_compile_options(-O3 -ffast-math)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|armv7")
            add_compile_options(-march=native)
        endif()
    endif()
endif()

# Dependencies
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(RTLSDR QUIET librtlsdr)
    pkg_check_modules(FFTW3 QUIET fftw3f)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
endif()
find_package(Threads REQUIRED)

# Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${RTLSDR_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${SQLITE3_INCLUDE_DIRS}
)

# Source Files
file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Main Executable
add_executable(radiosondepi ${SRC_FILES})

target_link_libraries(radiosondepi PRIVATE
    ${RTLSDR_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    Threads::Threads
)

if(WIN32)
    target_link_libraries(radiosondepi PRIVATE ws2_32)
endif()

# Sprint 1 Unit Tests
add_executable(test_sprint1
    tests/test_sprint1.cpp
    src/decoders/RS41Decoder.cpp
    src/sdr/RtlSdrDevice.cpp
)

target_link_libraries(test_sprint1 PRIVATE
    ${RTLSDR_LIBRARIES}
    ${FFTW3_LIBRARIES}
    Threads::Threads
)

if(WIN32)
    target_link_libraries(test_sprint1 PRIVATE ws2_32)
endif()

# Sprint 2 Unit Tests
add_executable(test_sprint2
    tests/test_sprint2.cpp
    src/decoders/DFMDecoder.cpp
    src/decoders/M10Decoder.cpp
    src/scanner/SpectrumScanner.cpp
)

target_link_libraries(test_sprint2 PRIVATE
    Threads::Threads
)

# Sprint 3 Unit Tests
add_executable(test_sprint3
    tests/test_sprint3.cpp
)

target_link_libraries(test_sprint3 PRIVATE
    Threads::Threads
)

if(WIN32)
    target_link_libraries(test_sprint3 PRIVATE ws2_32)
endif()

# Sprint 4 Unit Tests
add_executable(test_sprint4
    tests/test_sprint4.cpp
)

target_link_libraries(test_sprint4 PRIVATE
    Threads::Threads
)

enable_testing()
add_test(NAME Sprint1Tests COMMAND test_sprint1)
add_test(NAME Sprint2Tests COMMAND test_sprint2)
add_test(NAME Sprint3Tests COMMAND test_sprint3)
add_test(NAME Sprint4Tests COMMAND test_sprint4)
