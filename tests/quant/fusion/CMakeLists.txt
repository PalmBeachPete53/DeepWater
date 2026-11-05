# qtm_test_fusion: data fusion (src/fusion) on made-up sources: units read, normalisation, the relation of two
# sources (same, rebased, distinct, unverified, incompatible), conflicts, frequencies kept apart, priority.
#   cmake --build <build> --target qtm_test_fusion && <build>/tests-quant/fusion/qtm_test_fusion
add_executable(qtm_test_fusion test_fusion.cpp)
target_link_libraries(qtm_test_fusion PRIVATE fusion Qt6::Core)
