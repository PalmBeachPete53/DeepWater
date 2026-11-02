# qtm_test_pinets: Pine Script indicators through PineTS on Node.js (src/quant/core/PineScript.cpp,
# PineIndicators.cpp), checked against the C++ ta.* port. Needs Node.js; prints SKIPPED without it.
#   cmake --build <build> --target qtm_test_pinets && <build>/tests-quant/pinets/qtm_test_pinets
add_executable(qtm_test_pinets test_pinets.cpp)
target_link_libraries(qtm_test_pinets PRIVATE qtm_core Qt6::Core Qt6::Qml)
target_compile_definitions(qtm_test_pinets PRIVATE QTM_ROOT_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(qtm_test_pinets PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
