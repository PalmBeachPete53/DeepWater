# qtm_test_broker: the MetaTrader page's logic (src/broker): sizing an order per account, what refuses it, and its way out
# and back against a fake MetaCopier on the loopback address. `--serve <port>` runs that server alone, for trying the page:
#   DEEPWATER_METACOPIER_URL=http://127.0.0.1:<port> deepwater --page metatrader
add_executable(qtm_test_broker test_broker.cpp)
set_target_properties(qtm_test_broker PROPERTIES AUTOMOC ON)
target_link_libraries(qtm_test_broker PRIVATE broker desk Qt6::Core Qt6::Network)
