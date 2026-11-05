# qtm_test_update: what the update system reads from GitHub (src/UpdateRelease), on made-up answers.
#   cmake --build <build> --target qtm_test_update && <build>/tests-quant/update/qtm_test_update
add_executable(qtm_test_update test_update.cpp ${CMAKE_SOURCE_DIR}/src/UpdateRelease.cpp)
target_include_directories(qtm_test_update PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(qtm_test_update PRIVATE Qt6::Core)
