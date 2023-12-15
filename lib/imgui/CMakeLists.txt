#######################################################
#  This cmakelist handles the IMGUI library building  #
#######################################################

#  The imgui impl for the platform is in Renderer under include/src as imgui_impl_CrossRenderer  #

add_library(IMGUI STATIC
    "imgui.cpp"
    "imgui_draw.cpp"
    "imgui_demo.cpp"
    "imgui_widgets.cpp"
	"imgui_tables.cpp"

    "implot/implot.cpp"
    "implot/implot_demo.cpp"
    "implot/implot_items.cpp")

target_include_directories(IMGUI SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})