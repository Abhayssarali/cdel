cmake_minimum_required(VERSION 3.13)
project(DeadInstructionEliminator VERSION 1.0 LANGUAGES C CXX)

# ── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find LLVM ─────────────────────────────────────────────────────────────────
# Set LLVM_DIR via: cmake -DLLVM_DIR=$(llvm-config --cmakedir) ..
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# ── LLVM definitions and includes ─────────────────────────────────────────────
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# ── Include our own headers ───────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# ── Shared plugin library (loadable pass) ────────────────────────────────────
add_library(DeadInstructionEliminator MODULE
    src/DeadInstructionEliminator.cpp
)

# On Linux/macOS, LLVM passes are shared libraries
if(APPLE)
    set_target_properties(DeadInstructionEliminator PROPERTIES
        SUFFIX ".dylib"
    )
endif()

# Link LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS
    support
    core
    irreader
    passes
    analysis
    transformutils
)
# For a loadable module plugin in Linux, we do not link LLVM libraries statically to avoid option redefinition errors in opt
# target_link_libraries(DeadInstructionEliminator PRIVATE ${LLVM_LIBS})

# Suppress RTTI errors (LLVM is typically built without RTTI)
if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(DeadInstructionEliminator PRIVATE -fno-rtti)
endif()

# ── Standalone optimizer binary (optional) ───────────────────────────────────
add_executable(die-tool
    src/main.cpp
    src/DeadInstructionEliminator.cpp
)
target_include_directories(die-tool PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(die-tool PRIVATE ${LLVM_LIBS})
if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(die-tool PRIVATE -fno-rtti)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS DeadInstructionEliminator DESTINATION lib)
install(TARGETS die-tool DESTINATION bin)
