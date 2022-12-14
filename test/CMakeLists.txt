include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/58d77fa8070e8cec2dc1ed015d66b454c8d78850.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)

include_directories(../src/include)

add_executable(
        BasicTest
        ${TEST_SOURCES}
)

add_test(NAME BasicTest COMMAND BasicTest)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(RIORING rioringd)
else()
    set(RIORING rioring)
endif()

if (WIN32)
    target_link_options(BasicTest PUBLIC /LTCG)
    target_link_libraries(BasicTest PUBLIC ${RIORING} GTest::gtest_main)
else()
    target_link_libraries(BasicTest PUBLIC ${RIORING} uring GTest::gtest_main)
endif()


include(GoogleTest)
gtest_discover_tests(BasicTest)