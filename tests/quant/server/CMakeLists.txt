# qtm_test_server: the server-side tests of quant-terminal (providers, feeds, imports, ptq, server
# routes) ported to C++, plus a comparison with the JS outputs dumped by make_reference.mjs.
#   node tests/quant/server/make_reference.mjs     # after changing the JS or the cases
#   cmake --build <build> --target qtm_test_server && <build>/tests-quant/server/qtm_test_server

add_executable(qtm_test_server test_server.cpp)
set_target_properties(qtm_test_server PROPERTIES AUTOMOC ON)
target_link_libraries(qtm_test_server PRIVATE qtm_server qtm_core Qt6::Core Qt6::Network Qt6::HttpServer)
target_compile_definitions(qtm_test_server PRIVATE
    QTM_SERVER_REF="${CMAKE_CURRENT_SOURCE_DIR}/reference/reference.json"
    QTM_FIXTURES="${CMAKE_SOURCE_DIR}/tests/quant/fixtures"
    QTM_QT_ROOT="${CMAKE_SOURCE_DIR}/tests/quant/fixtures")

# The backend alone on a port, for the live comparison with `node server.js` and the examples.
set(QTM_SERVER_RUN_SOURCES ${QTM_SERVER_TEST_SOURCES})
list(REMOVE_ITEM QTM_SERVER_RUN_SOURCES test_server.cpp)
add_executable(qtm_server_run server_main.cpp ${QTM_SERVER_RUN_SOURCES})
target_link_libraries(qtm_server_run PRIVATE qtm_server qtm_core Qt6::Core)
target_compile_definitions(qtm_server_run PRIVATE QTM_QT_ROOT="${CMAKE_SOURCE_DIR}/tests/quant/fixtures")
