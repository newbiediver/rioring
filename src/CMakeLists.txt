set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LIBNAME rioringd)
else()
    set(LIBNAME rioring)
endif()

if (WIN32)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} /DNDEBUG /O2 /Oi /Oy /GL")
    add_definitions(/DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_AMD64_ /D_LIB /D_WIN32_WINNT=0x0A00)
    add_compile_options(/MP)
else()
    add_compile_options(-g -pthread -fthreadsafe-statics -fno-omit-frame-pointer -Wno-format-security)
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -D_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

file(GLOB_RECURSE SOURCE_FILES LIST_DIRECTORIES false *.h *.cpp)

include_directories(include)

add_library(${LIBNAME} STATIC ${SOURCE_FILES})