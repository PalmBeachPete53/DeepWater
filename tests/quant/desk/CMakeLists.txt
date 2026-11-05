# qtm_test_desk: the Desk and Journal tabs' logic (src/desk) on made-up data: position sizing, statements of MetaTrader 4
# and 5 and cTrader, journal statistics, currency indices, technical signals, stop pools, seasonality, risk appetite,
# fundamental scores, FX sessions, headline tags.
#   cmake --build <build> --target qtm_test_desk && <build>/tests-quant/desk/qtm_test_desk
add_executable(qtm_test_desk test_desk.cpp)
target_link_libraries(qtm_test_desk PRIVATE desk Qt6::Core)
