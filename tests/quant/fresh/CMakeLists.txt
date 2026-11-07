# qtm_test_fresh: how often each source is asked, what a published figure makes stale (src/fresh), and a real data set and
# research client against a fake server: a 304 costs nothing and redraws nothing, an answer already known isn't parsed.
add_executable(qtm_test_fresh test_fresh.cpp
    ${PROJECT_SOURCE_DIR}/src/MacroData.h ${PROJECT_SOURCE_DIR}/src/MacroData.cpp
    ${PROJECT_SOURCE_DIR}/src/HttpCache.h ${PROJECT_SOURCE_DIR}/src/HttpCache.cpp
    ${PROJECT_SOURCE_DIR}/src/ResearchData.h ${PROJECT_SOURCE_DIR}/src/ResearchData.cpp)
set_target_properties(qtm_test_fresh PROPERTIES AUTOMOC ON)
target_link_libraries(qtm_test_fresh PRIVATE fresh Qt6::Core Qt6::Network Qt6::Concurrent ZLIB::ZLIB)
target_include_directories(qtm_test_fresh PRIVATE ${PROJECT_SOURCE_DIR}/src)
