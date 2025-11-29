#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

#include "shader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

int main()
{
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(800, 600, "Terrain Renderer", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // ---------------- Heightmap loading ----------------
    stbi_set_flip_vertically_on_load(true); // optional

    const char* heightmapPath = "assets/heightmapper-1764410934226.png";
    int imgWidth  = 0;
    int imgHeight = 0;
    int imgChannels = 0;

    unsigned char* imageData = stbi_load(
        heightmapPath,
        &imgWidth,
        &imgHeight,
        &imgChannels,
        1   // force 1 channel (grayscale)
    );

    if (!imageData) {
        std::cerr << "Failed to load heightmap: " << heightmapPath << "\n";
        std::cerr << "stb_image reason: " << stbi_failure_reason() << "\n";
        glfwTerminate();
        return -1;
    } else {
        std::cout << "Loaded heightmap: " << heightmapPath << "\n";
        std::cout << "  Size: " << imgWidth << " x " << imgHeight << "\n";
        std::cout << "  Channels (requested): 1 (grayscale)\n";

        auto sample = [&](int x, int y) {
            int index = y * imgWidth + x;
            unsigned char value = imageData[index];
            std::cout << "  pixel(" << x << "," << y << ") = "
                      << static_cast<int>(value) << "\n";
        };

        int x0 = 0, y0 = 0;
        int x1 = imgWidth / 2, y1 = imgHeight / 2;
        int x2 = imgWidth - 1, y2 = imgHeight - 1;

        sample(x0, y0);
        sample(x1, y1);
        sample(x2, y2);
    }

    // ---------------- Build terrain mesh from heightmap ----------------

    // Subsample factor: take every Nth pixel to keep vertex count reasonable
    const int step = 8; // try 8 or 16; smaller = more detail, more vertices

    const int gridWidth  = imgWidth  / step;
    const int gridHeight = imgHeight / step;

    std::vector<float> vertices;          // 3 floats per vertex: x, y, z
    vertices.reserve(gridWidth * gridHeight * 3);

    float heightScale = 0.3f;             // vertical exaggeration

    for (int j = 0; j < gridHeight; ++j)       // j = row (y in image)
    {
        for (int i = 0; i < gridWidth; ++i)    // i = column (x in image)
        {
            int imgX = i * step;
            int imgY = j * step;

            int index = imgY * imgWidth + imgX;
            unsigned char hByte = imageData[index];
            float h = static_cast<float>(hByte) / 255.0f; // 0..1

            // Map i,j to X,Z in [-1, 1]
            float x = (static_cast<float>(i) / (gridWidth - 1)) * 2.0f - 1.0f;
            float z = (static_cast<float>(j) / (gridHeight - 1)) * 2.0f - 1.0f;

            float y = h * heightScale;

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }

    // We no longer need imageData once vertices are built (for this step)
    stbi_image_free(imageData);

    // Build index buffer (two triangles per grid cell)
    std::vector<unsigned int> indices;
    indices.reserve((gridWidth - 1) * (gridHeight - 1) * 6);

    for (int j = 0; j < gridHeight - 1; ++j)
    {
        for (int i = 0; i < gridWidth - 1; ++i)
        {
            unsigned int i0 = j * gridWidth + i;
            unsigned int i1 = j * gridWidth + (i + 1);
            unsigned int i2 = (j + 1) * gridWidth + i;
            unsigned int i3 = (j + 1) * gridWidth + (i + 1);

            // First triangle: i0, i2, i1
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            // Second triangle: i1, i2, i3
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    std::cout << "Terrain vertices: " << vertices.size() / 3 << "\n";
    std::cout << "Terrain triangles: " << indices.size() / 3 << "\n";

    // ---------------- OpenGL buffers for terrain mesh ----------------

    unsigned int terrainVAO, terrainVBO, terrainEBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glGenBuffers(1, &terrainEBO);

    glBindVertexArray(terrainVAO);

    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(float),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(unsigned int),
                 indices.data(),
                 GL_STATIC_DRAW);

    // position attribute: layout(location = 0) in vec3 aPos;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        (void*)0
    );

    glBindVertexArray(0);

    // ---------------- Shader & matrices ----------------

    Shader shader("shaders/vertex.glsl", "shaders/fragment.glsl");

    float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    shader.use();
    shader.setMat4("model", identity);
    shader.setMat4("view", identity);
    shader.setMat4("projection", identity);

    // Draw in wireframe to see the mesh
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // ---------------- Render loop ----------------
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glBindVertexArray(terrainVAO);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices.size()),
                       GL_UNSIGNED_INT,
                       (void*)0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup
    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &terrainEBO);

    glfwTerminate();
    return 0;
}
