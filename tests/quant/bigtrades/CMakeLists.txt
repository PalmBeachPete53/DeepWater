# qtm_test_bigtrades: Big Trades (src/quant/core/BigTrades.*) on hand-made trades whose results are worked out by hand
# in the comments: aggregation (time window, side, price, sweeps), tick-rule fallback, rolling percentile, manual and
# automatic thresholds, volatility factor, warm-up, bubble sizes.
#   cmake --build <build> --target qtm_test_bigtrades && <build>/tests-quant/bigtrades/qtm_test_bigtrades
add_executable(qtm_test_bigtrades test_bigtrades.cpp)
target_link_libraries(qtm_test_bigtrades PRIVATE qtm_core Qt6::Core)
target_include_directories(qtm_test_bigtrades PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
