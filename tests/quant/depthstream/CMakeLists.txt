# qtm_test_depthstream: the order book depth the chart panes share (src/quant/server/DepthStream.*) and each pane's side
# of it (src/quant/ui/DepthFeed.*), on a fake source that answers on the stream's thread and sends a book update every
# 50 ms of real time: one stream, socket and configuration per pair; one store of samples per interval; a pane joining
# late sees what is there; its stability filter built at once = built bit by bit; one recorder per pair; recordings
# read back at the start; a pane starting in replay; pauses; the last pane leaving. About 9 s (real time).
#   cmake --build <build> --target qtm_test_depthstream && <build>/tests-quant/depthstream/qtm_test_depthstream
add_executable(qtm_test_depthstream test_depthstream.cpp)
target_link_libraries(qtm_test_depthstream PRIVATE qtm_ui qtm_server qtm_core Qt6::Core)
target_include_directories(qtm_test_depthstream PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
