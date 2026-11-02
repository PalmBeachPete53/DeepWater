# qtm_panels_demo: the Quant Terminal (qtm::App) with commands that drive the side and bottom panels,
# for screenshots compared with the web app (panels.js, research.js, quantui.js, ptq.js).
#   qtm_panels_demo [--data DIR] [--custom DIR] [--w 1600 --h 980] [--wait MS]
#                   [--palette ID] [--do "command" [--after MS]]... [--shot out.png]
# --palette: one of deepwater's themes (ember, abyss, jade, oxide, glacier, dune), as the Charts page has it.
# Commands: side TAB | bottom TAB | collapse | alert | button TEXT | scroll N Y | theme | symbol X |
#   palette ID|none (switches deepwater theme live, like Settings does) |
#   waitprice SYMBOL (prints when the first quote for SYMBOL arrived) |
#   tf X | type TEXT (into the focused input) | enter | hover X Y | click X Y | shot FILE | dump
#   replay start N|step D|play|exit|speed X|sync on|off|pane N | layout L | bselect N VALUE |
#   tablebtn TEXT [DATA] | acceptnext (accept the next confirm box) | paperdump | toasts
#   csvfile PATH (the next Import bars/signal file dialog picks PATH) | csvtext (bottom panel texts) |
#   csvchart (each chart's symbol, tf, bar count) | csvind ID (adds indicator ID to the active chart) |
#   csvselect VALUE (the first bottom select with option VALUE gets it) |
#   pick bottom|side|win N VALUE (a user choice in the N-th visible select) | checkbox TEXT |
#   promptnext TEXT (answers the next prompt) | rejectnext (cancels the next confirm) | state |
#   appwidth W (forces the terminal to W px wide) | tooltip X Y (prints the tooltip at X,Y)
#   (scroll N Y: the N-th visible scroll area of the bottom panel, left to right, to Y px;
#    button TEXT: clicks the first visible push button with that text; dump: prints panel widgets)
add_executable(qtm_panels_demo main.cpp ${CMAKE_SOURCE_DIR}/src/Theme.cpp)
target_compile_features(qtm_panels_demo PRIVATE cxx_std_20)
target_link_libraries(qtm_panels_demo PRIVATE qtm_ui qtm_server qtm_core qtm_chart Qt6::Widgets)
target_include_directories(qtm_panels_demo PRIVATE ${CMAKE_SOURCE_DIR}/src/quant ${CMAKE_SOURCE_DIR})
target_compile_definitions(qtm_panels_demo PRIVATE QTM_QT_ROOT="${CMAKE_SOURCE_DIR}/tests/quant/fixtures")
set_target_properties(qtm_panels_demo PROPERTIES AUTOMOC ON)
