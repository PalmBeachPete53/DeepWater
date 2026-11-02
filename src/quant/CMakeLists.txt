# Quant Terminal: a C++ port of quant-terminal/ (the "Charts" page).
# Four libraries, one per layer, so each can be built and tested on its own:
#   qtm_core    public/lib/*.js       indicators, series, seasonals, backtest, quant checks, custom JS indicators
#   qtm_server  server.js + lib/*.js  data providers, feeds, imports, alerts, store, PTQ, local HTTP API
#   qtm_chart   Lightweight Charts    the chart engine (QPainter)
#   qtm_ui      public/js/*.js        the terminal UI (app, chart panes, drawings, panels)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets Svg Qml HttpServer Concurrent Multimedia)

file(GLOB QTM_CORE_SOURCES CONFIGURE_DEPENDS core/*.h core/*.cpp)
file(GLOB QTM_SERVER_SOURCES CONFIGURE_DEPENDS server/*.h server/*.cpp)
file(GLOB QTM_CHART_SOURCES CONFIGURE_DEPENDS chart/*.h chart/*.cpp)
file(GLOB QTM_UI_SOURCES CONFIGURE_DEPENDS ui/*.h ui/*.cpp)

add_library(qtm_core STATIC ${QTM_CORE_SOURCES})
target_compile_features(qtm_core PUBLIC cxx_std_20)
target_link_libraries(qtm_core PUBLIC Qt6::Core Qt6::Qml)
# Pine Script indicators run on PineTS (third_party/pinets, AGPL-3.0) in a Node.js child process; the
# bundle and the bridge script are compiled in and copied to the app's data folder on first use.
qt_add_resources(qtm_core pinets PREFIX /pinets BASE ${CMAKE_SOURCE_DIR}/third_party/pinets
    FILES ${CMAKE_SOURCE_DIR}/third_party/pinets/pinets.min.es.js ${CMAKE_SOURCE_DIR}/third_party/pinets/deepwater-pinets.mjs)

add_library(qtm_server STATIC ${QTM_SERVER_SOURCES})
target_compile_features(qtm_server PUBLIC cxx_std_20)
target_link_libraries(qtm_server PUBLIC qtm_core Qt6::Network Qt6::WebSockets Qt6::HttpServer Qt6::Concurrent)

add_library(qtm_chart STATIC ${QTM_CHART_SOURCES})
target_compile_features(qtm_chart PUBLIC cxx_std_20)
target_link_libraries(qtm_chart PUBLIC Qt6::Widgets)

add_library(qtm_ui STATIC ${QTM_UI_SOURCES})
target_compile_features(qtm_ui PUBLIC cxx_std_20)
target_link_libraries(qtm_ui PUBLIC qtm_core qtm_server qtm_chart Qt6::Widgets Qt6::Svg Qt6::Network Qt6::WebSockets Qt6::Multimedia)
# The Terminal tab (ui/Terminal.cpp): libvterm, built by the top-level CMakeLists.
target_link_libraries(qtm_ui PUBLIC vterm)
# Desktop notifications for alerts: UserNotifications on macOS (Objective-C++; OBJCXX is enabled at the top level),
# the system tray elsewhere (ui/Notifications.cpp, empty on macOS). The Terminal tab's pty is ConPTY on Windows.
if(APPLE)
    target_sources(qtm_ui PRIVATE ui/Notifications.mm)
    set_source_files_properties(ui/Notifications.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(qtm_ui PUBLIC "-framework UserNotifications" "-framework Foundation" "-framework AppKit")
endif()

foreach(lib qtm_core qtm_server qtm_chart qtm_ui)
    set_target_properties(${lib} PROPERTIES AUTOMOC ON)
endforeach()

# Tests and the side-by-side checks against the JavaScript originals (tests/quant).
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/quant/CMakeLists.txt)
    add_subdirectory(${CMAKE_SOURCE_DIR}/tests/quant ${CMAKE_BINARY_DIR}/tests-quant)
endif()
