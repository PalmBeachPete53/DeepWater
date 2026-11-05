# deepwater-llm for one architecture, as a project of its own: a universal build makes it once per architecture and joins
# the two with lipo (top-level CMakeLists.txt), since ggml picks its CPU code by the target architecture. A build for one
# architecture doesn't come through here.
cmake_minimum_required(VERSION 3.21)
project(deepwater-llm C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED_LIBS OFF)
foreach(_o LLAMA_BUILD_COMMON LLAMA_BUILD_TESTS LLAMA_BUILD_TOOLS LLAMA_BUILD_EXAMPLES LLAMA_BUILD_SERVER
           LLAMA_BUILD_APP LLAMA_ALL_WARNINGS LLAMA_OPENSSL
           GGML_NATIVE GGML_OPENMP GGML_BUILD_TESTS GGML_BUILD_EXAMPLES)
    set(${_o} OFF CACHE BOOL "" FORCE)
endforeach()
set(GGML_METAL ON CACHE BOOL "" FORCE)
set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)

# Without GGML_NATIVE, ggml's x86 code uses what it is told: the same level as the app's x86 slice.
if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    set(_v3 OFF)
    if(DEEPWATER_X86_LEVEL STREQUAL "v3")
        set(_v3 ON)
    endif()
    foreach(_o GGML_AVX GGML_AVX2 GGML_FMA GGML_F16C GGML_BMI2)
        set(${_o} ${_v3} CACHE BOOL "" FORCE)
    endforeach()
endif()

add_subdirectory(${LLAMA_DIR} llama EXCLUDE_FROM_ALL SYSTEM)
add_executable(deepwater-llm deepwater-llm.cpp)
target_link_libraries(deepwater-llm PRIVATE llama vendor::nlohmann)
