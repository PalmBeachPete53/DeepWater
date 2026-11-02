# qtm_test_navigator: the range slider under the charts. core/RangeSlider (hit zones, bounds, gestures) on hand-worked
# cases and 100,000 random gestures checked against its invariants; then ui/ChartNavigator on a real chart (offscreen):
# real mouse events, bursts of moves faster than the chart's frames, live bars, a compared series adding times, new
# data, a chart going away, cursors, the line's cache. Also ui/BarAxis (bars at their own chart index with a compared
# symbol), used by the footprint, Big Trades and the heatmap.
#   cmake --build <build> --target qtm_test_navigator && <build>/tests-quant/navigator/qtm_test_navigator
add_executable(qtm_test_navigator test_navigator.cpp)
target_link_libraries(qtm_test_navigator PRIVATE qtm_ui Qt6::Widgets)
target_include_directories(qtm_test_navigator PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
