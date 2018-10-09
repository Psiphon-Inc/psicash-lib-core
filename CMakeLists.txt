# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_STANDARD 14)

file (GLOB SOURCES "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*_test.cpp$")
file (GLOB TEST_SOURCES "*_test.cpp")
#message("SOURCES: ${SOURCES}")
#message("TEST_SOURCES: ${TEST_SOURCES}")

add_library( # Sets the name of the library.
             psicash

             # Sets the library as a static library.
             STATIC

             # Provides a relative path to your source file(s).
             ${SOURCES} )

SET(GCC_COVERAGE_COMPILE_FLAGS "-Wall -fprofile-arcs -ftest-coverage --coverage -g -O0")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")

#SET(GCC_COVERAGE_LINK_FLAGS    "-lgcov")
#SET(GCC_COVERAGE_LINK_FLAGS    "-lclang_rt.profile_osx -L/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/10.0.0/lib/darwin")
#SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")



### GTEST
# https://github.com/google/googletest/tree/master/googletest

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()


# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)


# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
        ${CMAKE_BINARY_DIR}/googletest-build
        EXCLUDE_FROM_ALL)


# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
    include_directories("${gtest_SOURCE_DIR}/include")
endif()

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(runUnitTests ${TEST_SOURCES})
target_link_libraries(runUnitTests gtest_main)
target_link_libraries(runUnitTests psicash)
#add_test(NAME example_test COMMAND runUnitTests)
