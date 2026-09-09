cmake_minimum_required(VERSION 3.20)

project(Euler1D_Cpp
    VERSION 1.0.0
    DESCRIPTION "Solver 1D de las ecuaciones de Euler por volumenes finitos tipo Godunov"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build" FORCE)
endif()

# -----------------------------------------------------------------------------
# Nucleo numerico
# -----------------------------------------------------------------------------
add_library(euler1d_core
    src/config/simulation_config.cpp
    src/config/namelist_parser.cpp
    src/initial/test_case.cpp
    src/boundary/boundary_condition.cpp
    src/numerical/cfl_controller.cpp
    src/numerical/finite_volume_operator.cpp
    src/numerical/reconstruction/reconstruction.cpp
    src/numerical/riemann/exact_riemann_solver.cpp
    src/numerical/source/source_term.cpp
    src/numerical/time/time_integrator.cpp
    src/io/fortran_format.cpp
    src/io/output_manager.cpp
    src/io/xl_reader.cpp
    src/simulation/simulation.cpp
    src/validation/error_analyzer.cpp)

target_include_directories(euler1d_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# NO se activa -ffast-math ni -Ofast: reasociarian las operaciones en coma
# flotante y romperian la equivalencia numerica con el Fortran, que es el
# criterio de aceptacion de esta migracion.
if(MSVC)
    target_compile_options(euler1d_core PRIVATE /W4 /fp:precise)
else()
    target_compile_options(euler1d_core PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

# Preparado para OpenMP pero DESACTIVADO por defecto: primero la version
# secuencial validada (ver docs/performance.md).
option(EULER1D_ENABLE_OPENMP "Compilar con OpenMP" OFF)
if(EULER1D_ENABLE_OPENMP)
    find_package(OpenMP REQUIRED)
    target_link_libraries(euler1d_core PUBLIC OpenMP::OpenMP_CXX)
endif()

# -----------------------------------------------------------------------------
# Ejecutables
# -----------------------------------------------------------------------------
add_executable(xEuler1D src/main.cpp)
target_link_libraries(xEuler1D PRIVATE euler1d_core)

add_executable(euler1d_compare src/tools/compare_runs.cpp)
target_link_libraries(euler1d_compare PRIVATE euler1d_core)

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
option(EULER1D_BUILD_TESTS "Compilar la suite de tests" ON)
if(EULER1D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
