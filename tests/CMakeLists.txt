# Cada archivo de test es un ejecutable independiente y se registra en CTest.
# El arnes esta en test_support.hpp (sin dependencias externas).

set(EULER1D_TESTS
    test_physics
    test_numerics
    test_exact_riemann
    test_config
    test_integration
    test_regression)

foreach(test_name IN LISTS EULER1D_TESTS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE euler1d_core)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(${test_name} PRIVATE /W4 /fp:precise)
    else()
        target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Las corridas archivadas del Fortran viven en el arbol de fuentes; el test de
# regresion las localiza por ruta absoluta para no depender del cwd de CTest.
target_compile_definitions(test_regression PRIVATE
    EULER1D_REFERENCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/reference")

# La regresion y la integracion recorren varias simulaciones completas.
set_tests_properties(test_regression test_integration PROPERTIES TIMEOUT 900)
