# qtm_test_klinestream: the live candles the chart panes share (src/quant/server/KlineStream.*) on a fake source: one
# stream per pair and timeframe, every subscription hearing its bars, a dropped connection opened again, no host at
# all (the panes poll) then a new pane trying again, the last subscription closing the stream; the timeframe mapping.
#   cmake --build <build> --target qtm_test_klinestream && <build>/tests-quant/klinestream/qtm_test_klinestream
add_executable(qtm_test_klinestream test_klinestream.cpp)
target_link_libraries(qtm_test_klinestream PRIVATE qtm_server qtm_core Qt6::Core)
target_include_directories(qtm_test_klinestream PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
