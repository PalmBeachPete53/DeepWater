# Visual and behavioural checks of the chart engine (qtm_chart). The side-by-side pages that rendered
# the real Lightweight Charts were removed with quant-terminal/; --export and --texts output is still
# there for when they come back.
#   qtm_chart_demo                 interactive window with all scenes
#   qtm_chart_demo --shot DIR      PNG of every scene (DIR/<scene>.png)
#   qtm_chart_demo --export DIR    the scene data as DIR/data.js for the reference pages
#   qtm_chart_demo --texts DIR [--scene S] [--dpr 2]  every label drawn (DIR/<scene>.json)
#   qtm_chart_demo --test          interaction / conversion checks, exit code 0 when all pass
#   qtm_chart_demo --bench         rendering time with 20,000 bars and many series
#   qtm_chart_demo --compare ours.png ref.png out.png [x y w h zoom]   stacked (zoomed) images
add_executable(qtm_chart_demo main.cpp Scenes.h Scenes.cpp)
target_compile_features(qtm_chart_demo PRIVATE cxx_std_20)
target_link_libraries(qtm_chart_demo PRIVATE qtm_chart Qt6::Widgets)
target_include_directories(qtm_chart_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/quant)
set_target_properties(qtm_chart_demo PROPERTIES AUTOMOC ON)
