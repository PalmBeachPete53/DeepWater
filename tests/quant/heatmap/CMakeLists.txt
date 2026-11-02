# qtm_test_heatmap: the liquidity heatmap's data (src/quant/core/OrderBook.*, Heatmap.*) on hand-made sequences of
# depth updates whose results are worked out in the comments: book rebuild, resyncs by snapshot, crossed books,
# sampling, the column store, row grouping, normalisation; then a load test (1,000 updates a second for a simulated
# minute: bounded memory, constant time per update).
#   cmake --build <build> --target qtm_test_heatmap && <build>/tests-quant/heatmap/qtm_test_heatmap
add_executable(qtm_test_heatmap test_heatmap.cpp)
target_link_libraries(qtm_test_heatmap PRIVATE qtm_core Qt6::Core)
target_include_directories(qtm_test_heatmap PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
