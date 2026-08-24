cmake_minimum_required(VERSION 3.15)
project(CPUSimulator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPU_SIM_BUILD_GUI "Build the native ImGui/ImPlot frontend" ON)

set(IMGUI_SOURCES
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/imgui_demo.cpp
        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_opengl3.cpp
)

# -------------------------------------------------
# ImPlot sources
# -------------------------------------------------
set(IMPLOT_SOURCES
        external/implot/implot.cpp
        external/implot/implot_items.cpp
        external/implot/implot_demo.cpp
)

# -------------------------------------------------
# CPU core library
# -------------------------------------------------
add_library(cpu_core
        src/cpu_core.cpp
        src/instr.cpp
        src/stages.cpp
        src/memory.cpp
)

target_include_directories(cpu_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -------------------------------------------------
# Build target selection
# -------------------------------------------------
set(IS_EMSCRIPTEN FALSE)
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(IS_EMSCRIPTEN TRUE)
endif()

# -------------------------------------------------
# Native builds: CLI + GUI
# -------------------------------------------------
if (NOT IS_EMSCRIPTEN)
    # CLI / TUI executable
    add_executable(rv32-sim
            src/main.cpp
    )
    target_link_libraries(rv32-sim PRIVATE
            cpu_core
    )

    if (CPU_SIM_BUILD_GUI)
        find_package(OpenGL REQUIRED)

        # GLFW: build only the library from the vendored source tree.
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    # GLFW: build from vendored source
    add_subdirectory(external/glfw)

    # GUI executable: rv32-gui
    add_executable(rv32-gui
            src/gui_main.cpp
            ${IMGUI_SOURCES}
            ${IMPLOT_SOURCES}
    )

    target_include_directories(rv32-gui PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
            ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
            ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/include
            ${CMAKE_CURRENT_SOURCE_DIR}/external/implot
    )

    target_link_libraries(rv32-gui PRIVATE
            cpu_core
            glfw
            OpenGL::GL
    )
    endif()

    include(CTest)
    if (BUILD_TESTING)
        add_test(NAME example_array_sum
                 COMMAND rv32-sim ${CMAKE_CURRENT_SOURCE_DIR}/programs/test.hex)
        add_test(NAME example_fibonacci
                 COMMAND rv32-sim ${CMAKE_CURRENT_SOURCE_DIR}/web/examples/fibonacci.hex)
        add_test(NAME example_factorial
                 COMMAND rv32-sim ${CMAKE_CURRENT_SOURCE_DIR}/web/examples/factorial.hex)
        add_test(NAME example_bubble_sort
                 COMMAND rv32-sim ${CMAKE_CURRENT_SOURCE_DIR}/web/examples/bubble_sort.hex)
        add_test(NAME example_load_use_hazard
                 COMMAND rv32-sim ${CMAKE_CURRENT_SOURCE_DIR}/web/examples/hazard.hex)
        add_test(NAME example_branch_prediction
                 COMMAND rv32-sim ${CMAKE_CURRENT_SOURCE_DIR}/web/examples/branch_pred.hex)
    endif()

endif()

# -------------------------------------------------
# WebAssembly build: CPU core exposed to a web UI
# -------------------------------------------------
if (IS_EMSCRIPTEN)
    add_executable(rv32-wasm
            src/wasm_api.cpp
    )
    target_link_libraries(rv32-wasm PRIVATE cpu_core)

    # SINGLE_FILE below embeds the WASM payload in web/sim.js.
    set_target_properties(rv32-wasm PROPERTIES
            OUTPUT_NAME "sim"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web"
    )

    # Modern ES module factory + allow memory growth; keep it simple (no embind)
    target_link_options(rv32-wasm PRIVATE
            "SHELL:-sMODULARIZE=1"
            "SHELL:-sEXPORT_ES6=1"
            "SHELL:-sENVIRONMENT=web"
            "SHELL:-sALLOW_MEMORY_GROWTH=1"
            "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
            "SHELL:-sEXIT_RUNTIME=0"
            "SHELL:-sASSERTIONS=0"
            "SHELL:-sSINGLE_FILE=1"
    )
endif()
