# qtm_test_core: checks qtm_core (series, indicators, seasonals, custom indicators) against the
# outputs of the JavaScript originals, dumped by make_reference.mjs into reference/reference.json.
#   node tests/quant/core/make_reference.mjs     # after changing the JS or the cases
#   cmake --build <build> --target qtm_test_core && <build>/tests-quant/core/qtm_test_core
add_executable(qtm_test_core test_core.cpp)
target_link_libraries(qtm_test_core PRIVATE qtm_core Qt6::Core Qt6::Qml)
target_compile_definitions(qtm_test_core PRIVATE
    QTM_REF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/reference"
    QTM_ROOT_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(qtm_test_core PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
