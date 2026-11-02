# qtm_ui_demo: the whole Quant Terminal (qtm::App) in a window, on a qtm::Backend with its own data
# and custom folders, for trying the UI and for screenshots compared with the web app.
#   qtm_ui_demo [--data DIR] [--custom DIR] [--state state.json] [--w 1600 --h 980]
#               [--wait MS] [--do "command" [--after MS]]... [--shot out.png]
# Without --data the data folder is a temp dir (removed on exit); the custom folder is seeded with
# quant-terminal/custom/ema-ribbon-squeeze.js. --shot renders the window at 1x and quits.
# Commands (--do): layout X | theme | tf X | type X | symbol X | compare X | log | ind ID |
#   modal indicators|symbol|settings|indsettings N|compare | replay N | step N | play | exit-replay |
#   tool X | click X Y | wclick X Y | move X Y | key K | toast TEXT | hover X Y | active N | esc | shot FILE |
#   draw TOOL X1 Y1 [X2 Y2 [X3 Y3]] | select X Y | delete | indsrc ID PARENT_INDEX OUTPUT [k v]... |
#   rmind N | hideind N | settings KEY VALUE | picker KEY | csv FILE | snap FILE | range | focus OBJNAME | inds
# (X Y are coordinates in the active chart, in pixels.)
add_executable(qtm_ui_demo main.cpp)
target_compile_features(qtm_ui_demo PRIVATE cxx_std_20)
target_link_libraries(qtm_ui_demo PRIVATE qtm_ui qtm_server qtm_core qtm_chart Qt6::Widgets)
target_include_directories(qtm_ui_demo PRIVATE ${CMAKE_SOURCE_DIR}/src/quant)
target_compile_definitions(qtm_ui_demo PRIVATE QTM_QT_ROOT="${CMAKE_SOURCE_DIR}/tests/quant/fixtures")
set_target_properties(qtm_ui_demo PROPERTIES AUTOMOC ON)
