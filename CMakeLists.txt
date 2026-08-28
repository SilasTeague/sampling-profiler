cmake_minimum_required(VERSION 3.20)
project(strobe CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(STROBE_BUILD_CLI "Build the standalone report-printing CLI (strobe)" ON)
option(STROBE_BUILD_TESTS "Build the sampler test suite" ON)

# sampler.cpp relies on Darwin/arm64-only APIs (mach, backtrace() unwinding
# through _sigtramp) -- it's macOS-only. Callers should guard usage of
# strobe::sampler with `if (TARGET strobe::sampler)`.
if (APPLE)
	add_library(strobe_sampler sampler.cpp)
	target_include_directories(strobe_sampler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
	# Public: backtrace() must unwind through the *caller's* frames too, so
	# every consumer needs this, not just sampler.cpp itself.
	target_compile_options(strobe_sampler PUBLIC -fno-omit-frame-pointer)
	add_library(strobe::sampler ALIAS strobe_sampler)
endif()

add_library(strobe_aggregator aggregator.cpp)
target_include_directories(strobe_aggregator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_library(strobe::aggregator ALIAS strobe_aggregator)

if (STROBE_BUILD_CLI)
	add_executable(strobe main.cpp)
	target_link_libraries(strobe PRIVATE strobe::aggregator)
endif()

if (STROBE_BUILD_TESTS AND APPLE)
	enable_testing()

	add_executable(test_sampler_accuracy tests/test_sampler_accuracy.cpp)
	target_link_libraries(test_sampler_accuracy PRIVATE strobe::sampler)
	add_test(NAME test_sampler_accuracy COMMAND test_sampler_accuracy)

	add_executable(test_leaf_visibility tests/test_leaf_visibility.cpp)
	target_link_libraries(test_leaf_visibility PRIVATE strobe::sampler)
	add_test(NAME test_leaf_visibility COMMAND test_leaf_visibility)

	# Both tests write to a hardcoded "tests/..." relative path, so they need
	# the source tree (not ctest's default build-dir cwd) as their cwd.
	set_tests_properties(test_sampler_accuracy test_leaf_visibility PROPERTIES
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
