cmake_minimum_required(VERSION 3.10)
project(PBRObjViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(glfw3_DIR "C:/Program Files (x86)/GLFW/lib/cmake/glfw3")

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)

# GLAD
#add_library(glad STATIC glad/include/glad/glad.h glad/src/glad.c)
add_library(glad glad/include/glad/glad.h glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

# Dear ImGui (vendored source + GLFW/OpenGL3 backends).
# The OpenGL3 backend ships its own function loader, so it coexists with glad.
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

include_directories(${CMAKE_SOURCE_DIR})

# Main executable
add_executable(pbr_viewer 
    main.cpp
    glad/src/glad.c
)

target_link_libraries(pbr_viewer
    glfw
    OpenGL::GL
    glad
    glm::glm
    imgui
)

# Windows-specific: link necessary system libraries
if(WIN32)
    target_link_libraries(pbr_viewer 
        kernel32 
        user32 
        gdi32 
        winmm
        imm32 
        ole32 
        oleaut32 
        version
    )
endif()

# Copy shaders to build directory
#file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})
#file(COPY models DESTINATION ${CMAKE_BINARY_DIR})

# Replace the file(COPY) lines with this:
add_custom_command(TARGET pbr_viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:pbr_viewer>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/models $<TARGET_FILE_DIR:pbr_viewer>/models
)
