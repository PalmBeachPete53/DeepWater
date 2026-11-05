# qtm_test_tradestream: the Binance trades the chart panes share (src/quant/server/TradeStream.*) and each pane's side of
# them (src/quant/ui/TradeFeed.*), on a fake source whose trades and answers the test controls: history page by page to
# the budget or the first bar, live trades, gaps filled from REST, a gap longer than the budget, binance.us, failed
# requests, several panes on one stream (one socket, one history), pauses, the last pane leaving; each pane's
# footprint, built bit by bit, against the same trades built at once.
#   cmake --build <build> --target qtm_test_tradestream && <build>/tests-quant/tradestream/qtm_test_tradestream
add_executable(qtm_test_tradestream test_tradestream.cpp)
target_link_libraries(qtm_test_tradestream PRIVATE qtm_ui qtm_server qtm_core Qt6::Core)
target_include_directories(qtm_test_tradestream PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
