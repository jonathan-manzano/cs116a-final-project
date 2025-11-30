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

    // tells GLFW what kind of OpenGL context to request when creating the window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(
        800,                    // window width in pixels
        600,                   // window height in pixels
        "Terrain Renderer",            // window title (shown in title bar)
        nullptr,         // fullscreen monitor (nullptr = windowed)
        nullptr           // Window to share OpenGL context with (nullptr = no sharing)
    );

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

     // aligns png/jpeg image formats with OpenGL's coordinate system. 
     // Image formats usually have the origin at the top-left corner, 
     // while OpenGL has the origin at the bottom left
    stbi_set_flip_vertically_on_load(true);

    const char* heightmapPath = "assets/heightmapper-1764410934226.png";
    int imgWidth  = 0;
    int imgHeight = 0;
    int imgChannels = 0; // a channel is one color component per pixel. ex: 1 channel: Grayscale, 3: RGB, 4: RGBA

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
    const int step = 8; // smaller step = more vertices

    // dimensions of vertex grid
    const int gridWidth  = imgWidth  / step;
    const int gridHeight = imgHeight / step;

    std::vector<float> vertices;          // 3 floats per vertex: x, y, z
    vertices.reserve(gridWidth * gridHeight * 3);

    float heightScale = 0.3f;             // vertical exaggeration

    // loop through every position in the vertex grid
    // sample the corresponding pixel in the heightmap
    // map grid position to 3D X, Z coordinates
    // convert pixel brightness to height (Y coord)
    for (int j = 0; j < gridHeight; ++j)       // j = row (y in image)
    {
        for (int i = 0; i < gridWidth; ++i)    // i = column (x in image)
        {
            // convert grid position to image coordinates
            int imgX = i * step;
            int imgY = j * step;

            int index = imgY * imgWidth + imgX; //convert 2D coordinate to 1D index for the image data array
            unsigned char hByte = imageData[index]; // get pixel value (brightness)
            float h = static_cast<float>(hByte) / 255.0f; // normalize to 0-1 range

            // map grid position to 3D X, Z coordinates
            // [-1, 1] range, because OpenGL clip space is that range
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

    // Build index buffer that defines which vertices form triangles.
    // instead of storing vertex data multiple times (since triangles can share vertices), 
    // we store each vertex once and use indices to reference them
    std::vector<unsigned int> indices;
    indices.reserve((gridWidth - 1) * (gridHeight - 1) * 6); // # of grid cells * 6 indices per cell (2 triangles with 3 vertices each)

    // iterate through each grid cell (NOT vertex position like the first loop)
    // a grid cell is a square formed by 4 adjacent vertices 
    for (int j = 0; j < gridHeight - 1; ++j)
    {
        for (int i = 0; i < gridWidth - 1; ++i)
        {
            // calculate the indices of the 4 corners of the current grid cell
            // i0-i4 are indices in the 1D vertices array
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

    // VAO - Vertex Array Object: Container that stores vertex attribute configurations and buffer bindings. The settings for how vertex data shouls be read by the GPU
    // VBO - Vertex Buffer Object: Container that stores vertex data (ex: positions and colors)
    // EBO - Element Buffer Object: Container that stores indices
    unsigned int terrainVAO, terrainVBO, terrainEBO;

    // creates 1 of each needed array/buffer object and stores their IDs in the above vars
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glGenBuffers(1, &terrainEBO);

    // bind terrainVAO so that the following vertex attribure configuration are stored in it
    glBindVertexArray(terrainVAO);

    // upload vertex data to GPU
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(float),
                 vertices.data(),
                 GL_STATIC_DRAW);

    // upload index data to GPU
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(unsigned int),
                 indices.data(),
                 GL_STATIC_DRAW);

    // position attribute: layout(location = 0) in vec3 aPos;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer( // tell OpenGL how to read the data
        0,            // attribute location
        3,              // 3 components per vertex
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        (void*)0
    );

    glBindVertexArray(0); //unbind terrainVAO when not needed for best practice

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

    // draws only edges instead of filled triangles for now
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // ---------------- Render loop ----------------
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f); //background color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use(); //sets the active shader program in OpenGL's state. glDrawElements knows to use this shader
        glBindVertexArray(terrainVAO); // bind again, so OpenGL knows which vertices and indices to use which we set earlier
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices.size()),
                       GL_UNSIGNED_INT,
                       (void*)0);

        glfwSwapBuffers(window);
        glfwPollEvents();  // processes window events (keyboard, mouse, window close, etc.)
    }

    // cleanup
    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &terrainEBO);

    glfwTerminate();
    return 0;
}
