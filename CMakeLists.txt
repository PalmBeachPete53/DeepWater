cmake_minimum_required(VERSION 3.21)
project(Deepwater VERSION 0.1 LANGUAGES C CXX)
# Objective-C++ only on macOS: the article reader's WebKit host and the native notifications.
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network WebSockets Concurrent WebEngineWidgets LinguistTools)
# World Bank, USDA and JODI bulk downloads are zip archives. macOS and Linux have zlib; on Windows a vcpkg zlib is
# used when the toolchain provides one, otherwise zlib's sources are fetched and built statically.
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    include(FetchContent)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(zlib GIT_REPOSITORY https://github.com/madler/zlib.git GIT_TAG v1.3.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(zlib)
    target_include_directories(zlibstatic INTERFACE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

# Windows (MSVC): the sources are UTF-8 (typographic quotes, dashes, arrows in strings), use M_PI, and must not
# get min/max macros from <windows.h>.
if(MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8;/permissive-;/Zc:__cplusplus;/bigobj>"
                        "$<$<COMPILE_LANGUAGE:C>:/utf-8>")
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

# The indicator, backtest and chart code is heavy on number crunching: build optimised by default.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# libvterm 0.3.3 (MIT, third_party/libvterm): the terminal emulator behind the Charts page's
# Terminal tab. Vendored unchanged from https://www.leonerd.org.uk/code/libvterm/.
file(GLOB VTERM_SOURCES CONFIGURE_DEPENDS third_party/libvterm/src/*.c)
add_library(vterm STATIC ${VTERM_SOURCES})
set_target_properties(vterm PROPERTIES C_STANDARD 11)
target_include_directories(vterm PUBLIC third_party/libvterm/include PRIVATE third_party/libvterm/src)
target_compile_options(vterm PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)

add_subdirectory(src/quant)

# Markov models of the Quant page (HMM regimes, MS-GARCH, execution MDP): a pure engine on Qt Core,
# no UI, tested by tests/quant/markov.
file(GLOB MARKOV_SOURCES CONFIGURE_DEPENDS src/markov/*.h src/markov/*.cpp)
add_library(markov STATIC ${MARKOV_SOURCES})
target_compile_features(markov PUBLIC cxx_std_20)
target_link_libraries(markov PUBLIC Qt6::Core)
target_include_directories(markov PUBLIC src)

qt_add_executable(deepwater MACOSX_BUNDLE WIN32
    src/main.cpp
    src/MarketData.h src/MarketData.cpp
    src/VesselStore.h src/VesselStore.cpp
    src/AisStream.h src/AisStream.cpp
    src/Chokepoints.h src/Chokepoints.cpp
    src/ChokepointsPage.h src/ChokepointsPage.cpp
    src/SeaRoutes.h src/SeaRoutes.cpp
    src/PortWatch.h src/PortWatch.cpp
    src/SupplyChain.h src/SupplyChain.cpp
    src/DetailPanelSupply.cpp
    src/FxData.h src/FxData.cpp
    src/HttpCache.h src/HttpCache.cpp
    src/MacroData.h src/MacroData.cpp
    src/DomesticFinance.h src/DomesticFinance.cpp
    src/KeyIndicators.h src/KeyIndicators.cpp
    src/OfficialSources.h src/OfficialSources.cpp
    src/NationalSources.h src/NationalSources.cpp
    src/KeyedSources.h src/KeyedSources.cpp
    src/SurveyHeadlines.h src/SurveyHeadlines.cpp
    src/AiModels.h src/AiModels.cpp
    src/ClaudeClient.h src/ClaudeClient.cpp
    src/AiPage.h src/AiPage.cpp
    src/MlEnvironment.h src/MlEnvironment.cpp
    src/MlData.h src/MlData.cpp
    src/AiMlView.h src/AiMlView.cpp
    src/CommodityPrices.h src/CommodityPrices.cpp
    src/ForexPage.h src/ForexPage.cpp
    src/GeoData.h src/GeoData.cpp
    src/GlobeWidget.h src/GlobeWidget.cpp
    src/Sparkline.h src/Sparkline.cpp
    src/LeftPanel.h src/LeftPanel.cpp
    src/DetailPanel.h src/DetailPanel.cpp
    src/MacroIndicators.h src/MacroIndicators.cpp
    src/MacroPanel.h src/MacroPanel.cpp
    src/HeaderBar.h src/HeaderBar.cpp
    src/DevTools.h src/DevTools.cpp
    src/CodeGraphView.h src/CodeGraphView.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/Theme.h src/Theme.cpp
    src/SettingsDialog.h src/SettingsDialog.cpp
    src/SourcesPage.h src/SourcesPage.cpp
    src/CalendarData.h src/CalendarData.cpp
    src/CalendarPage.h src/CalendarPage.cpp
    src/ResearchData.h src/ResearchData.cpp
    src/ResearchCharts.h src/ResearchCharts.cpp
    src/ResearchViews.h src/ResearchCompanyViews.cpp src/ResearchMarketViews.cpp
    src/ResearchPage.h src/ResearchPage.cpp
    src/QuantData.h src/QuantData.cpp
    src/QuantCharts.h src/QuantCharts.cpp
    src/QuantViews.h src/QuantCompareView.cpp src/QuantPairViews.cpp src/QuantCotView.cpp src/QuantSignalsView.cpp
    src/QuantJob.h src/QuantJob.cpp
    src/QuantMarketState.h src/QuantMarketState.cpp
    src/QuantMarkovJobs.h src/QuantMarkovJobs.cpp
    src/QuantPortfolioView.cpp
    src/QuantMarkovViews.h src/QuantMarkovView.cpp src/QuantMarkovHmmView.cpp src/QuantMarkovGarchView.cpp src/QuantMarkovExecutionView.cpp
    src/QuantPage.h src/QuantPage.cpp
    src/OverviewWidgets.h src/OverviewWidgets.cpp
    src/OverviewPage.h src/OverviewPage.cpp
    src/EconomyData.h src/EconomyData.cpp
    src/EconomyPage.h src/EconomyPage.cpp
    src/CountryData.h src/CountryData.cpp
    src/CountriesView.h src/CountriesView.cpp
    src/NewsData.h src/NewsData.cpp
    src/NewsPage.h src/NewsPage.cpp
    src/NewsReader.h src/NewsReader.cpp
    src/AdBlock.h src/AdBlock.cpp
    src/NewsWebHost.h src/NewsWebHost.cpp src/NewsWebHostQt.cpp
    resources/resources.qrc
    resources/icon/deepwater.icns
)

target_link_libraries(deepwater PRIVATE Qt6::Widgets Qt6::Network Qt6::WebSockets Qt6::Concurrent Qt6::WebEngineWidgets qtm_ui markov ZLIB::ZLIB)

# On macOS the article reader renders with the system's WebKit (WKWebView), the engine of Safari.
if(APPLE)
    target_sources(deepwater PRIVATE src/NewsWebHostMac.mm)
    set_source_files_properties(src/NewsWebHostMac.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(deepwater PRIVATE "-framework WebKit")
endif()

# The article reader's ad blocker: Brave's adblock-rust, behind a small C ABI (third_party/adblock-ffi), built by
# cargo into a static library. Without a Rust toolchain the reader simply doesn't block anything.
find_program(CARGO cargo HINTS $ENV{HOME}/.cargo/bin $ENV{USERPROFILE}/.cargo/bin)
if(CARGO)
    set(ADBLOCK_DIR ${CMAKE_SOURCE_DIR}/third_party/adblock-ffi)
    set(ADBLOCK_LIB ${CMAKE_BINARY_DIR}/adblock-ffi/release/${CMAKE_STATIC_LIBRARY_PREFIX}adblock_ffi${CMAKE_STATIC_LIBRARY_SUFFIX})
    add_custom_command(OUTPUT ${ADBLOCK_LIB}
        COMMAND ${CARGO} build --release --quiet --manifest-path ${ADBLOCK_DIR}/Cargo.toml --target-dir ${CMAKE_BINARY_DIR}/adblock-ffi
        DEPENDS ${ADBLOCK_DIR}/Cargo.toml ${ADBLOCK_DIR}/src/lib.rs
        COMMENT "Building adblock-rust (the reader's ad blocker)")
    add_custom_target(adblock_ffi DEPENDS ${ADBLOCK_LIB})
    add_dependencies(deepwater adblock_ffi)
    target_include_directories(deepwater PRIVATE ${ADBLOCK_DIR}/include)
    target_link_libraries(deepwater PRIVATE ${ADBLOCK_LIB})
    target_compile_definitions(deepwater PRIVATE DEEPWATER_ADBLOCK)
    if(WIN32)   # what a Rust static library needs from Windows
        target_link_libraries(deepwater PRIVATE ws2_32 userenv bcrypt ntdll advapi32)
    endif()
else()
    message(STATUS "cargo not found: the article reader is built without the ad blocker")
endif()

# A bundle identifier is needed for desktop notifications (UserNotifications); it matches the
# QSettings domain.
set_target_properties(deepwater PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER com.deepwater.Deepwater)

# App icon: resources/icon/deepwater.svg is the source; deepwater.icns is rendered from it.
set_source_files_properties(resources/icon/deepwater.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(deepwater PROPERTIES MACOSX_BUNDLE_ICON_FILE deepwater.icns)
# The linker's ad-hoc signature names the binary ("deepwater"), and usernotificationsd refuses an app
# whose signed identifier doesn't match its bundle identifier: sign the whole bundle (ad-hoc) again.
if(APPLE)
    add_custom_command(TARGET deepwater POST_BUILD
        COMMAND codesign --force --sign - $<TARGET_BUNDLE_DIR:deepwater>
        COMMENT "Signing deepwater.app (ad-hoc)")
endif()

# Windows: the icon (resources/icon/deepwater.ico, rendered from the same PNG) and a folder the app runs from on any
# PC: `cmake --install build --prefix <dir>` copies deepwater.exe and runs windeployqt (Qt DLLs, plugins, the
# WebEngine process and its resources, translations).
if(WIN32)
    target_sources(deepwater PRIVATE resources/icon/deepwater.rc)
    install(TARGETS deepwater RUNTIME DESTINATION .)
    qt_generate_deploy_app_script(TARGET deepwater OUTPUT_SCRIPT deepwater_deploy NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${deepwater_deploy})
endif()

# The Charts page (Quant Terminal port) seeds its custom indicator folder from here on first run.
qt_add_resources(deepwater quant PREFIX /quant BASE resources/quant FILES resources/quant/ema-ribbon-squeeze.js)
# The machine-learning scripts the AI tab copies into the app's Python environment.
qt_add_resources(deepwater ml PREFIX /ml BASE resources/ml FILES resources/ml/train.py resources/ml/check_env.py)

# Translations. deepwater_fr.ts holds UI strings and is refreshed from the
# sources with `cmake --build build --target deepwater_lupdate`.
# deepwater_data_fr.ts holds strings from markets.json (context "MarketData")
# and is maintained by hand, since lupdate cannot see them.
qt_add_lupdate(TS_FILES i18n/deepwater_fr.ts SOURCE_TARGETS deepwater LUPDATE_TARGET deepwater_lupdate)
qt_add_lrelease(TS_FILES i18n/deepwater_fr.ts i18n/deepwater_data_fr.ts
                LRELEASE_TARGET deepwater_lrelease QM_FILES_OUTPUT_VARIABLE qm_files)
qt_add_resources(deepwater translations PREFIX /i18n BASE ${CMAKE_CURRENT_BINARY_DIR} FILES ${qm_files})
add_dependencies(deepwater deepwater_lrelease)
