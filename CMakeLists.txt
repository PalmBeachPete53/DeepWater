cmake_minimum_required(VERSION 3.21)
# macOS: the oldest system the app runs on. Unset, it is the build machine's own macOS, which no Intel Mac has past
# macOS 26. A build that names its architectures (CMAKE_OSX_ARCHITECTURES: the CI, a universal binary) is one to
# distribute, against Qt's online installer: it gets 14.4, Qt 6.12's minimum. A plain build for this Mac keeps the
# host's version, since Homebrew's Qt is itself built for it. Must be set before project().
if(CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.4 CACHE STRING "Minimum macOS version" FORCE)
endif()
project(Deepwater VERSION 0.1 LANGUAGES C CXX)
# Objective-C++ on macOS: the article reader's WebKit host and the native notifications; Objective-C for ggml's Metal
# backend (the local engine), whose .m files would otherwise be compiled as Objective-C++.
if(APPLE)
    enable_language(OBJC OBJCXX)
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

# Optimised builds (every configuration but Debug, which stays as CMake makes it). README › Build options.
set(_dw_opt "$<NOT:$<CONFIG:Debug>>")
if(APPLE)
    # The architectures built: CMAKE_OSX_ARCHITECTURES ("arm64;x86_64" for a universal binary), else the host's.
    set(DEEPWATER_ARCHS ${CMAKE_OSX_ARCHITECTURES})
    if(NOT DEEPWATER_ARCHS)
        set(DEEPWATER_ARCHS ${CMAKE_HOST_SYSTEM_PROCESSOR})
    endif()
    # The CPU each slice is tuned for. arm64: the M1, which every Apple silicon Mac can run. x86_64: v3 (AVX2, FMA,
    # BMI2), which every Intel Mac that runs macOS 14 has (2018 and later); v2 for older x86 or a stricter baseline.
    set(DEEPWATER_X86_LEVEL v3 CACHE STRING "x86-64 micro-architecture level of the x86_64 slice (v2 or v3)")
    set_property(CACHE DEEPWATER_X86_LEVEL PROPERTY STRINGS v2 v3)
    set(_dw_cpu_arm64 -mcpu=apple-m1)
    set(_dw_cpu_x86_64 -march=x86-64-${DEEPWATER_X86_LEVEL})
    foreach(_a IN LISTS DEEPWATER_ARCHS)
        # -Xarch_<arch>: the flag goes to that slice's compilation only, in a universal build as in a single one.
        add_compile_options("$<${_dw_opt}:SHELL:-Xarch_${_a} ${_dw_cpu_${_a}}>")
    endforeach()
endif()

# Link-time optimisation across the whole program (the app, qtm_*, markov, vterm): ThinLTO with Clang.
option(DEEPWATER_LTO "Link-time optimisation (ThinLTO with Clang) in optimised builds" ON)
if(DEEPWATER_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _dw_ipo OUTPUT _dw_ipo_error LANGUAGES C CXX)
    if(_dw_ipo)
        foreach(_c RELEASE RELWITHDEBINFO MINSIZEREL)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_${_c} ON)
        endforeach()
        if(APPLE)   # ThinLTO's cache: a rebuild re-optimises only the modules that changed
            add_link_options("$<${_dw_opt}:LINKER:-cache_path_lto,${CMAKE_BINARY_DIR}/lto-cache>")
        endif()
    else()
        message(STATUS "LTO not supported, built without it: ${_dw_ipo_error}")
    endif()
endif()

# Profile-guided optimisation, off by default: GENERATE builds an instrumented binary, cmake/pgo-train.sh runs it on
# the benchmarks and merges <arch>.profdata files into DEEPWATER_PGO_DIR, USE builds with them (one profile per slice).
set(DEEPWATER_PGO OFF CACHE STRING "Profile-guided optimisation: OFF, GENERATE or USE")
set_property(CACHE DEEPWATER_PGO PROPERTY STRINGS OFF GENERATE USE)
set(DEEPWATER_PGO_DIR ${CMAKE_SOURCE_DIR}/build-pgo-profiles CACHE PATH "Folder of the <arch>.profdata files")
if(DEEPWATER_PGO STREQUAL "GENERATE")
    add_compile_options("$<${_dw_opt}:-fprofile-instr-generate>")
    add_link_options("$<${_dw_opt}:-fprofile-instr-generate>")
elseif(DEEPWATER_PGO STREQUAL "USE")
    foreach(_a IN LISTS DEEPWATER_ARCHS)
        if(NOT EXISTS ${DEEPWATER_PGO_DIR}/${_a}.profdata)
            message(FATAL_ERROR "DEEPWATER_PGO=USE: ${DEEPWATER_PGO_DIR}/${_a}.profdata is missing (cmake/pgo-train.sh)")
        endif()
        add_compile_options("$<${_dw_opt}:SHELL:-Xarch_${_a} -fprofile-instr-use=${DEEPWATER_PGO_DIR}/${_a}.profdata>")
    endforeach()
    # Code the training didn't run, or that changed since, is optimised without a profile: no warning for each.
    add_compile_options("$<${_dw_opt}:-Wno-profile-instr-unprofiled;-Wno-profile-instr-out-of-date>")
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

# Data fusion (src/fusion): several sources of an indicator made into one golden record. Qt Core only, tested by
# tests/quant/fusion.
file(GLOB FUSION_SOURCES CONFIGURE_DEPENDS src/fusion/*.h src/fusion/*.cpp)
add_library(fusion STATIC ${FUSION_SOURCES})
target_compile_features(fusion PUBLIC cxx_std_20)
target_link_libraries(fusion PUBLIC Qt6::Core)
target_include_directories(fusion PUBLIC src)

# The Desk and Journal tabs' logic (src/desk): sizing, statements, statistics, market readings. Qt Core only, tested by
# tests/quant/desk.
file(GLOB DESK_SOURCES CONFIGURE_DEPENDS src/desk/*.h src/desk/*.cpp)
add_library(desk STATIC ${DESK_SOURCES})
target_compile_features(desk PUBLIC cxx_std_20)
target_link_libraries(desk PUBLIC Qt6::Core)
target_include_directories(desk PUBLIC src)

# How often to ask each source, and what a published figure makes stale (src/fresh). Tested by tests/quant/fresh.
file(GLOB FRESH_SOURCES CONFIGURE_DEPENDS src/fresh/*.h src/fresh/*.cpp)
add_library(fresh STATIC ${FRESH_SOURCES})
target_compile_features(fresh PUBLIC cxx_std_20)
target_link_libraries(fresh PUBLIC Qt6::Core)
target_include_directories(fresh PUBLIC src)

# The MetaTrader page's logic (src/broker): orders sized per account, sent through MetaCopier. Tested by tests/quant/broker.
file(GLOB BROKER_SOURCES CONFIGURE_DEPENDS src/broker/*.h src/broker/*.cpp)
add_library(broker STATIC ${BROKER_SOURCES})
target_compile_features(broker PUBLIC cxx_std_20)
set_target_properties(broker PROPERTIES AUTOMOC ON)
target_link_libraries(broker PUBLIC Qt6::Core Qt6::Network desk)
target_include_directories(broker PUBLIC src)

# Local models (AI › Local): llama.cpp b12070 (MIT, third_party/llama.cpp: only what libllama needs, unchanged), built
# static with Metal, behind deepwater-llm (src/llm), a helper process the app starts in a Seatbelt sandbox
# (src/LocalEngine.cpp). macOS only. ggml picks its CPU code by the target architecture, so a universal build makes it
# once per architecture (src/llm/CMakeLists.txt) and joins the two with lipo.
set(DEEPWATER_LLM OFF)
set(DEEPWATER_LLM_UNIVERSAL OFF)
if(APPLE)
    list(LENGTH DEEPWATER_ARCHS _dw_narch)
    if(_dw_narch EQUAL 1)
        set(DEEPWATER_LLM ON)
    else()
        set(DEEPWATER_LLM_UNIVERSAL ON)
    endif()
elseif(WIN32)   # in an AppContainer (src/LocalSandboxWin.cpp), on the GPU through Vulkan when the Vulkan SDK is there
    set(DEEPWATER_LLM ON)
endif()
if(DEEPWATER_LLM_UNIVERSAL)
    include(ExternalProject)
    set(_dw_llm_slices)
    foreach(_a IN LISTS DEEPWATER_ARCHS)
        ExternalProject_Add(deepwater-llm-${_a}
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/llm
            BINARY_DIR ${CMAKE_BINARY_DIR}/llm-${_a}
            CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=${_a}
                       -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
                       -DCMAKE_CXX_FLAGS=${_dw_cpu_${_a}} -DCMAKE_C_FLAGS=${_dw_cpu_${_a}}
                       -DLLAMA_DIR=${CMAKE_SOURCE_DIR}/third_party/llama.cpp
                       -DDEEPWATER_X86_LEVEL=${DEEPWATER_X86_LEVEL}
            BUILD_ALWAYS OFF
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/llm-${_a}/deepwater-llm)
        list(APPEND _dw_llm_slices ${CMAKE_BINARY_DIR}/llm-${_a}/deepwater-llm)
    endforeach()
    # Inside the bundle, next to deepwater (Contents/MacOS), as in a build for one architecture.
    set(_dw_llm ${CMAKE_BINARY_DIR}/deepwater.app/Contents/MacOS/deepwater-llm)
    add_custom_command(OUTPUT ${_dw_llm}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/deepwater.app/Contents/MacOS
        COMMAND lipo -create ${_dw_llm_slices} -output ${_dw_llm}
        COMMAND codesign --force --sign - ${_dw_llm}   # lipo leaves it unsigned, and signing the bundle then fails
        DEPENDS ${_dw_llm_slices}
        COMMENT "Joining deepwater-llm's slices")
    add_custom_target(deepwater-llm DEPENDS ${_dw_llm})
    foreach(_a IN LISTS DEEPWATER_ARCHS)
        add_dependencies(deepwater-llm deepwater-llm-${_a})
    endforeach()
endif()
if(DEEPWATER_LLM)
    set(BUILD_SHARED_LIBS OFF)
    foreach(_o LLAMA_BUILD_COMMON LLAMA_BUILD_TESTS LLAMA_BUILD_TOOLS LLAMA_BUILD_EXAMPLES LLAMA_BUILD_SERVER
               LLAMA_BUILD_APP LLAMA_ALL_WARNINGS LLAMA_OPENSSL
               GGML_NATIVE GGML_OPENMP GGML_BUILD_TESTS GGML_BUILD_EXAMPLES)
        set(${_o} OFF CACHE BOOL "" FORCE)
    endforeach()
    if(APPLE)
        set(GGML_METAL ON CACHE BOOL "" FORCE)
        set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
    else()   # what every PC that runs Windows 11 has
        foreach(_o GGML_AVX GGML_AVX2 GGML_FMA GGML_F16C GGML_BMI2)
            set(${_o} ON CACHE BOOL "" FORCE)
        endforeach()
        # The GPU, whichever its maker, with the CPU as ever behind it: without a driver Vulkan finds no device. The helper
        # links the Vulkan loader, which not every PC has, so the loader is installed beside it.
        find_package(Vulkan QUIET COMPONENTS glslc)
        if(Vulkan_FOUND AND Vulkan_GLSLC_EXECUTABLE)
            set(GGML_VULKAN ON CACHE BOOL "" FORCE)
            find_file(DEEPWATER_VULKAN_LOADER vulkan-1.dll
                      PATHS $ENV{VULKAN_SDK}/Runtime/x64 $ENV{VULKAN_SDK}/Bin $ENV{SystemRoot}/System32 NO_DEFAULT_PATH)
            if(DEEPWATER_VULKAN_LOADER)
                install(FILES ${DEEPWATER_VULKAN_LOADER} DESTINATION .)
            else()
                message(FATAL_ERROR "The Vulkan SDK is there but not its vulkan-1.dll")
            endif()
        else()
            message(STATUS "No Vulkan SDK: the local engine runs on the CPU only")
        endif()
    endif()
    add_subdirectory(third_party/llama.cpp EXCLUDE_FROM_ALL SYSTEM)
    add_executable(deepwater-llm src/llm/deepwater-llm.cpp)
    target_link_libraries(deepwater-llm PRIVATE llama vendor::nlohmann)
    # Next to deepwater, where the app finds it: in the bundle (Contents/MacOS), installed with it; on Windows, installed below.
    set_target_properties(deepwater-llm PROPERTIES AUTOMOC OFF AUTORCC OFF
        RUNTIME_OUTPUT_DIRECTORY $<IF:$<BOOL:${APPLE}>,${CMAKE_BINARY_DIR}/deepwater.app/Contents/MacOS,${CMAKE_BINARY_DIR}>)
    if(WIN32)
        target_link_libraries(deepwater-llm PRIVATE ws2_32)   # its sandbox self-test tries the network
        install(TARGETS deepwater-llm RUNTIME DESTINATION .)
    endif()
endif()

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
    src/UpdateRelease.h src/UpdateRelease.cpp src/Updater.h src/Updater.cpp src/UpdateDialog.h src/UpdateDialog.cpp
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
    src/SideFlap.h src/SideFlap.cpp
    src/CodeGraphView.h src/CodeGraphView.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/Theme.h src/Theme.cpp
    src/SettingsDialog.h src/SettingsDialog.cpp
    src/SourcesPage.h src/SourcesPage.cpp
    src/SourceFlow.h src/SourceFlow.cpp
    src/ModelSlots.h src/ModelSlots.cpp
    src/LocalEngine.h src/LocalEngine.cpp
    src/AiLocalView.h src/AiLocalView.cpp
    src/LocalChat.h src/LocalChat.cpp
    src/OpencodeRun.h src/OpencodeRun.cpp
    src/AiRead.h
    src/AiOrganize.h src/AiOrganize.cpp
    src/AiDock.h src/AiDock.cpp
    src/LocalTools.h src/LocalTools.cpp
    src/ToolJournal.h src/ToolJournal.cpp
    src/McpServer.h src/McpServer.cpp
    src/AiOpencodeView.h src/AiOpencodeView.cpp
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
    src/RefreshScheduler.h src/RefreshScheduler.cpp
    src/ReleaseWatcher.h src/ReleaseWatcher.cpp
    src/CountryData.h src/CountryData.cpp
    src/CountriesView.h src/CountriesView.cpp
    src/IndicatorData.h src/IndicatorData.cpp
    src/IndicatorsView.h src/IndicatorsView.cpp
    src/NewsData.h src/NewsData.cpp
    src/NewsPage.h src/NewsPage.cpp
    src/NewsReader.h src/NewsReader.cpp
    src/AdBlock.h src/AdBlock.cpp
    src/NewsWebHost.h src/NewsWebHost.cpp src/NewsWebHostQt.cpp
    src/Voice.h
    src/DeskStrip.h src/DeskStrip.cpp
    src/Myfxbook.h src/Myfxbook.cpp
    src/PageTranslator.h src/PageTranslator.cpp
    src/HelpWindow.h src/HelpWindow.cpp
    src/LayoutAudit.h src/LayoutAudit.cpp
    src/MetaTraderPage.h src/MetaTraderPage.cpp
    src/SecretStore.h src/SecretStore.cpp
    src/DeskHistory.h src/DeskHistory.cpp
    src/DeskWidgets.h src/DeskWidgets.cpp
    src/JournalData.h src/JournalData.cpp
    src/JournalPage.h src/JournalPage.cpp
    src/DeskData.h src/DeskData.cpp
    src/DeskPage.h src/DeskPage.cpp
    src/DeskViewsIndicators.cpp src/DeskViews.h src/DeskViewsMarket.cpp src/DeskViewsMacro.cpp src/DeskViewsTech.cpp
    resources/resources.qrc
    resources/icon/deepwater.icns
)

target_link_libraries(deepwater PRIVATE broker fresh)
target_link_libraries(deepwater PRIVATE Qt6::Widgets Qt6::Network Qt6::WebSockets Qt6::Concurrent Qt6::WebEngineWidgets qtm_ui markov fusion desk ZLIB::ZLIB)
# The commit this build comes from, for the update system (src/Updater.cpp) and --version.
set(DEEPWATER_BUILDINFO ${CMAKE_BINARY_DIR}/generated/BuildInfo.h)
add_custom_target(deepwater_buildinfo
    COMMAND ${CMAKE_COMMAND} -DSOURCE=${CMAKE_SOURCE_DIR} -DOUT=${DEEPWATER_BUILDINFO} -P ${CMAKE_SOURCE_DIR}/cmake/BuildInfo.cmake
    BYPRODUCTS ${DEEPWATER_BUILDINFO}
    COMMENT "Reading the commit")
add_dependencies(deepwater deepwater_buildinfo)
target_include_directories(deepwater PRIVATE ${CMAKE_BINARY_DIR}/generated)
if(DEEPWATER_LLM OR DEEPWATER_LLM_UNIVERSAL)   # built before the app, so that the bundle is signed with it inside
    add_dependencies(deepwater deepwater-llm)
endif()

# On macOS the article reader renders with the system's WebKit (WKWebView), the engine of Safari.
if(APPLE)
    target_sources(deepwater PRIVATE src/NewsWebHostMac.mm src/VoiceMac.mm)
    set_source_files_properties(src/NewsWebHostMac.mm src/VoiceMac.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(deepwater PRIVATE "-framework Security" "-framework CoreFoundation")
    target_link_libraries(deepwater PRIVATE "-framework WebKit" "-framework Speech" "-framework AVFoundation")
elseif(WIN32)
    target_sources(deepwater PRIVATE src/VoiceWin.cpp)
    target_link_libraries(deepwater PRIVATE sapi ole32 userenv advapi32)
else()
    target_sources(deepwater PRIVATE src/VoiceStub.cpp)
endif()
# The local engine's Windows sandbox. Listed everywhere, compiled on Windows: lupdate only reads a target's sources.
target_sources(deepwater PRIVATE src/LocalSandboxWin.h src/LocalSandboxWin.cpp)
if(NOT WIN32)
    set_source_files_properties(src/LocalSandboxWin.cpp PROPERTIES HEADER_FILE_ONLY ON)
endif()

# The article reader's ad blocker: Brave's adblock-rust, behind a small C ABI (third_party/adblock-ffi), built by
# cargo into a static library. Without a Rust toolchain the reader simply doesn't block anything.
find_program(CARGO cargo HINTS $ENV{HOME}/.cargo/bin $ENV{USERPROFILE}/.cargo/bin)
if(CARGO)
    set(ADBLOCK_DIR ${CMAKE_SOURCE_DIR}/third_party/adblock-ffi)
    set(ADBLOCK_LIB ${CMAKE_BINARY_DIR}/adblock-ffi/release/${CMAKE_STATIC_LIBRARY_PREFIX}adblock_ffi${CMAKE_STATIC_LIBRARY_SUFFIX})
    if(APPLE)
        # One cargo build per architecture, for the same CPU and macOS as the C++ code, then one universal library.
        set(_dw_rust_arm64 aarch64-apple-darwin)
        set(_dw_rust_x86_64 x86_64-apple-darwin)
        set(_dw_rust_cpu_arm64 apple-m1)
        set(_dw_rust_cpu_x86_64 x86-64-${DEEPWATER_X86_LEVEL})
        set(_dw_rust_cmds)
        set(_dw_rust_libs)
        set(_dw_rust_env)
        if(CMAKE_OSX_DEPLOYMENT_TARGET)
            set(_dw_rust_env MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
        endif()
        foreach(_a IN LISTS DEEPWATER_ARCHS)
            set(_t ${_dw_rust_${_a}})
            string(TOUPPER ${_t} _T)
            string(REPLACE "-" "_" _T ${_T})
            # /usr/bin/env rather than cmake -E env, which doesn't pass make's jobserver on to cargo
            list(APPEND _dw_rust_cmds COMMAND env ${_dw_rust_env}
                "$<${_dw_opt}:CARGO_TARGET_${_T}_RUSTFLAGS=-Ctarget-cpu=${_dw_rust_cpu_${_a}}>"
                ${CARGO} build --release --quiet --target ${_t} --manifest-path ${ADBLOCK_DIR}/Cargo.toml
                --target-dir ${CMAKE_BINARY_DIR}/adblock-ffi)
            list(APPEND _dw_rust_libs ${CMAKE_BINARY_DIR}/adblock-ffi/${_t}/release/libadblock_ffi.a)
        endforeach()
        add_custom_command(OUTPUT ${ADBLOCK_LIB}
            ${_dw_rust_cmds}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/adblock-ffi/release
            COMMAND lipo -create ${_dw_rust_libs} -output ${ADBLOCK_LIB}
            DEPENDS ${ADBLOCK_DIR}/Cargo.toml ${ADBLOCK_DIR}/src/lib.rs
            COMMENT "Building adblock-rust (the reader's ad blocker) for ${DEEPWATER_ARCHS}")
    else()
        add_custom_command(OUTPUT ${ADBLOCK_LIB}
            COMMAND ${CARGO} build --release --quiet --manifest-path ${ADBLOCK_DIR}/Cargo.toml --target-dir ${CMAKE_BINARY_DIR}/adblock-ffi
            DEPENDS ${ADBLOCK_DIR}/Cargo.toml ${ADBLOCK_DIR}/src/lib.rs
            COMMENT "Building adblock-rust (the reader's ad blocker)")
    endif()
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
# Qt's Info.plist, plus why the AI panel's push-to-talk asks for the microphone and speech recognition.
set_target_properties(deepwater PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in)

# App icon: resources/icon/deepwater.svg is the source; deepwater.icns is rendered from it.
set_source_files_properties(resources/icon/deepwater.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(deepwater PROPERTIES MACOSX_BUNDLE_ICON_FILE deepwater.icns)
# deepwater-translate: the reader's on-device translation (Apple's Translation framework), a small Swift helper next to
# deepwater in the bundle, compiled once per architecture and joined with lipo. It needs swiftc; without it the reader falls
# back on the local model.
if(APPLE)
    find_program(SWIFTC swiftc)
    if(SWIFTC)
        if(CMAKE_OSX_DEPLOYMENT_TARGET)
            set(_dw_swift_min ${CMAKE_OSX_DEPLOYMENT_TARGET})
        else()
            set(_dw_swift_min 14.4)
        endif()
        set(_dw_translate ${CMAKE_BINARY_DIR}/deepwater-translate-joined)   # not the target's name, which Make would loop on
        set(_dw_translate_slices)
        set(_dw_translate_build)
        foreach(_a IN LISTS DEEPWATER_ARCHS)
            list(APPEND _dw_translate_slices ${_dw_translate}-${_a})
            list(APPEND _dw_translate_build COMMAND ${SWIFTC} -O -target ${_a}-apple-macos${_dw_swift_min}
                 -o ${_dw_translate}-${_a} ${CMAKE_CURRENT_SOURCE_DIR}/src/translate-helper.swift)
        endforeach()
        add_custom_command(OUTPUT ${_dw_translate}
            ${_dw_translate_build}
            COMMAND lipo -create ${_dw_translate_slices} -output ${_dw_translate}
            COMMAND codesign --force --sign - ${_dw_translate}   # lipo leaves it unsigned
            DEPENDS src/translate-helper.swift
            COMMENT "Building deepwater-translate")
        add_custom_target(deepwater-translate DEPENDS ${_dw_translate})
        add_dependencies(deepwater deepwater-translate)
        add_custom_command(TARGET deepwater POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${_dw_translate} $<TARGET_FILE_DIR:deepwater>/deepwater-translate
            COMMENT "Copying deepwater-translate into the bundle")
    endif()
endif()

# The linker's ad-hoc signature names the binary ("deepwater"), and usernotificationsd refuses an app
# whose signed identifier doesn't match its bundle identifier: sign the whole bundle (ad-hoc) again.
if(APPLE)
    add_custom_command(TARGET deepwater POST_BUILD
        COMMAND codesign --force --sign - $<TARGET_BUNDLE_DIR:deepwater>
        COMMENT "Signing deepwater.app (ad-hoc)")
endif()

# Windows: the executable icon (resources/icon/deepwater.ico, rendered from the same PNG).
if(WIN32)
    target_sources(deepwater PRIVATE resources/icon/deepwater.rc)
endif()

# Install and packaging. `cmake --install build --prefix <dir>` gives an app that runs on a machine without Qt:
# deepwater.app with the Qt frameworks inside (macdeployqt) on macOS, deepwater.exe next to the Qt DLLs, plugins,
# QtWebEngineProcess and its resources (windeployqt) on Windows. `cmake --build build --target dist` builds the app
# and packs that folder with CPack into build/dist: a .dmg on macOS, an installer .exe (NSIS) and a portable .zip on
# Windows. .github/workflows/build.yml runs it on GitHub's macOS and Windows machines at every push.
if(APPLE)
    install(TARGETS deepwater BUNDLE DESTINATION .)
else()
    # Flat, as windeployqt lays it out: deepwater.exe, the Qt DLLs and qt.conf at the top, plugins/, translations/,
    # resources/ beside. Qt's deploy script puts the DLLs in QT_DEPLOY_BIN_DIR, which defaults to the
    # CMAKE_INSTALL_BINDIR seen by find_package(Qt6) ("bin"), where Windows wouldn't look for them next to an exe at
    # the top; the script keeps a value set before it runs.
    install(TARGETS deepwater RUNTIME DESTINATION .)
    install(CODE "set(QT_DEPLOY_BIN_DIR .)")
    # The Visual C++ runtime (msvcp140.dll, vcruntime140*.dll) next to the exe, which Microsoft allows: a fresh
    # Windows 11 may not have it, and the portable .zip has no installer to run vc_redist. The UCRT is part of
    # Windows 10 and later.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)
endif()
qt_generate_deploy_app_script(TARGET deepwater OUTPUT_SCRIPT deepwater_deploy NO_UNSUPPORTED_PLATFORM_ERROR
                              NO_COMPILER_RUNTIME)
install(SCRIPT ${deepwater_deploy})
if(APPLE)
    # Library paths macdeployqt leaves pointing at Homebrew, then the ad-hoc signature again: macdeployqt rewrites
    # load paths, which voids signatures, and an Apple silicon Mac refuses to load an unsigned binary.
    list(LENGTH DEEPWATER_ARCHS _narch)
    if(_narch EQUAL 1)   # a universal build (arm64;x86_64) keeps both halves
        install(CODE "set(DEEPWATER_ARCH ${DEEPWATER_ARCHS})")
    endif()
    install(SCRIPT cmake/MacBundleFix.cmake)
endif()

# The version in the package names; the CI build sets it from the git tag (v1.2 → 1.2) or the commit.
set(DEEPWATER_PACKAGE_VERSION ${PROJECT_VERSION} CACHE STRING "Version in the .dmg / .exe / .zip names")
set(CPACK_PACKAGE_NAME Deepwater)
set(CPACK_PACKAGE_VERSION ${DEEPWATER_PACKAGE_VERSION})
set(CPACK_PACKAGE_VENDOR Deepwater)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Markets, macro and shipping terminal")
set(CPACK_PACKAGE_INSTALL_DIRECTORY Deepwater)
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/dist)
set(CPACK_STRIP_FILES OFF)
if(APPLE)
    set(CPACK_GENERATOR DragNDrop)          # deepwater.app and a link to /Applications
    set(CPACK_DMG_VOLUME_NAME Deepwater)
    set(CPACK_DMG_FORMAT UDBZ)
    if(_narch GREATER 1)
        set(CPACK_PACKAGE_FILE_NAME "Deepwater-${DEEPWATER_PACKAGE_VERSION}-macOS-universal")
    else()
        set(CPACK_PACKAGE_FILE_NAME "Deepwater-${DEEPWATER_PACKAGE_VERSION}-macOS-${DEEPWATER_ARCHS}")
    endif()
elseif(WIN32)
    set(CPACK_GENERATOR NSIS ZIP)
    set(CPACK_PACKAGE_FILE_NAME "Deepwater-${DEEPWATER_PACKAGE_VERSION}-windows-x64")
    set(CPACK_NSIS_PACKAGE_NAME Deepwater)
    set(CPACK_NSIS_DISPLAY_NAME Deepwater)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/resources/icon/deepwater.ico)
    set(CPACK_NSIS_MUI_UNIICON ${CMAKE_SOURCE_DIR}/resources/icon/deepwater.ico)
    set(CPACK_NSIS_INSTALLED_ICON_NAME deepwater.exe)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY .)  # deepwater.exe sits at the root of the install folder, not in bin
    set(CPACK_PACKAGE_EXECUTABLES deepwater Deepwater)   # Start menu shortcut
    set(CPACK_CREATE_DESKTOP_LINKS deepwater)
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN deepwater.exe)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()
include(CPack)
get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
add_custom_target(dist
    COMMAND ${CMAKE_CPACK_COMMAND} --config ${CMAKE_BINARY_DIR}/CPackConfig.cmake
            $<$<BOOL:${_multi}>:-C> $<$<BOOL:${_multi}>:$<CONFIG>>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS deepwater
    USES_TERMINAL
    COMMENT "Packaging Deepwater into build/dist")

# The Charts page (Quant Terminal port) seeds its custom indicator folder from here on first run.
qt_add_resources(deepwater quant PREFIX /quant BASE resources/quant FILES resources/quant/ema-ribbon-squeeze.js)
# The documentation (the header's ? icon): one Markdown file per chapter and language, resources/help/<lang>/<id>.md.
file(GLOB help_files CONFIGURE_DEPENDS resources/help/*/*.md)
qt_add_resources(deepwater help PREFIX /help BASE resources/help FILES ${help_files})
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
