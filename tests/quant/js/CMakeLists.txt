# qtm_test_js: modern JavaScript in custom indicators (JsTransform + polyfills) against Node.
#   node tests/quant/js/make_reference.mjs     # after changing features.mjs, indicators/ or the cases
#   cmake --build <build> --target qtm_test_js && <build>/tests-quant/js/qtm_test_js
add_executable(qtm_test_js test_js.cpp)
target_link_libraries(qtm_test_js PRIVATE qtm_core Qt6::Core Qt6::Qml)
target_compile_definitions(qtm_test_js PRIVATE QTM_ROOT_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(qtm_test_js PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
