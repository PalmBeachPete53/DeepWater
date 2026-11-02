# qtm_test_markov: checks the Markov engine (src/markov): parameter recovery on simulated data,
# numerical stability, the MDP against Almgren-Chriss, and estimation times.
#   cmake --build <build> --target qtm_test_markov && <build>/tests-quant/markov/qtm_test_markov
add_executable(qtm_test_markov test_markov.cpp)
target_link_libraries(qtm_test_markov PRIVATE markov Qt6::Core)
