# qtm_test_footprint: the footprint aggregation (src/quant/core/Footprint.*) on hand-made trades whose results are
# worked out by hand in the comments: side classification (feed, quotes, tick rule), ticks and rows, delta, POC,
# diagonal imbalances (at the candle's edges too), stacked imbalances, incremental vs rebuilt.
#   cmake --build <build> --target qtm_test_footprint && <build>/tests-quant/footprint/qtm_test_footprint
add_executable(qtm_test_footprint test_footprint.cpp)
target_link_libraries(qtm_test_footprint PRIVATE qtm_core Qt6::Core)
target_include_directories(qtm_test_footprint PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
