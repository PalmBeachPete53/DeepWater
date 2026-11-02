# qtm_test_backtest: checks the port of quant-terminal/public/lib/{backtest,quant}.js against the
# JS originals (reference/reference.json, written by make_reference.mjs), plus the JS test suite
# (test/backtest.test.js, test/quant.test.js) ported as it is.
#   node tests/quant/backtest/make_reference.mjs     # after changing the JS or the cases
#   cmake --build <build> --target qtm_test_backtest && <build>/tests-quant/backtest/qtm_test_backtest
add_executable(qtm_test_backtest test_backtest.cpp)
target_link_libraries(qtm_test_backtest PRIVATE qtm_core Qt6::Core)
target_compile_definitions(qtm_test_backtest PRIVATE
    QTM_REF="${CMAKE_CURRENT_SOURCE_DIR}/reference/reference.json"
    QTM_FIXTURES="${CMAKE_SOURCE_DIR}/tests/quant/fixtures")
target_include_directories(qtm_test_backtest PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
