# One test program per layer. Each agent/maintainer adds its own target here:
#   qtm_test_core     (core vs Node reference data)   -> tests/quant/core/
#   qtm_test_backtest (backtest + quant checks)       -> tests/quant/backtest/
#   qtm_test_server   (providers parsing, router)     -> tests/quant/server/
#   qtm_chart_demo    (chart engine visual checks)    -> tests/quant/chart/
file(GLOB QTM_TEST_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach(dir ${QTM_TEST_DIRS})
    if(IS_DIRECTORY ${dir} AND EXISTS ${dir}/CMakeLists.txt)
        add_subdirectory(${dir})
    endif()
endforeach()
