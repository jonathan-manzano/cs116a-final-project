cmake_minimum_required(VERSION 3.21...3.30)
project(TerrainRenderer)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(libs/glfw)

include_directories(
    libs/glad/include
    libs/glm
    libs/glfw/include
    include
)

file(GLOB SRC src/*.cpp libs/glad/src/gl.c)

add_executable(TerrainRenderer ${SRC})
target_link_libraries(TerrainRenderer glfw ${GLFW_LIBRARIES})

if (APPLE)
    target_link_libraries(TerrainRenderer "-framework OpenGL")
elseif(UNIX)
    find_package(OpenGL REQUIRED)
    target_link_libraries(TerrainRenderer OpenGL::GL)
elseif(WIN32)
    target_link_libraries(TerrainRenderer opengl32)
endif()
