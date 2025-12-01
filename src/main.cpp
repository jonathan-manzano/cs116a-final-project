#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// --- camera settings ---
glm::vec3 cameraPos   = glm::vec3(0.0f, 1.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw   = -90.0f;   // facing -Z
float pitch = -20.0f;   // slightly looking down
float fov   = 45.0f;

float lastX = 400.0f;   // window center (half of 800)
float lastY = 300.0f;   // window center (half of 600)
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// wireframe mode toggle
bool wireframeMode = false;
bool tKeyPressed = false;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window);

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

    glEnable(GL_DEPTH_TEST);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // capture mouse cursor for camera
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    // ---------------- Heightmap loading ----------------

     // aligns png/jpeg image formats with OpenGL's coordinate system. 
     // Image formats usually have the origin at the top-left corner, 
     // while OpenGL has the origin at the bottom left

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

    // 6 floats per vertex: x, y, z, nx, ny, nz (normals)
    std::vector<float> vertices(gridWidth * gridHeight * 6, 0.0f);

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

            int imgIndex = imgY * imgWidth + imgX; //convert 2D coordinate to 1D index for the image data array
            unsigned char hByte = imageData[imgIndex]; // get pixel value (brightness)
            float h = static_cast<float>(hByte) / 255.0f; // normalize to 0-1 range

            // map grid position to 3D X, Z coordinates
            // [-1, 1] range, because OpenGL clip space is that range
            float x = (static_cast<float>(i) / (gridWidth - 1)) * 2.0f - 1.0f;
            float z = (static_cast<float>(j) / (gridHeight - 1)) * 2.0f - 1.0f;
            float y = h * heightScale;

            int vIndex = j * gridWidth + i; // vertex index in grid
            int base = vIndex * 6;

            vertices[base] = x;
            vertices[base + 1] = y;
            vertices[base + 2] = z;
        }
    }

    // We no longer need imageData once vertices are built (for this step)
    stbi_image_free(imageData);

    // -----Compute Normals from heights in vertices---------
    auto getHeight = [&](int i, int j) -> float {
        i = std::max(0, std::min(i, gridWidth  - 1));
        j = std::max(0, std::min(j, gridHeight - 1));
        int vIdx = j * gridWidth + i;
        return vertices[vIdx * 6 + 1]; // Y component
    };
    
    // iterate through vertex grid positions
    // get height of neighboring pixels to get the slopes
    // cross product the slopes to get the normal, store in vertices list
    for (int j = 0; j < gridHeight; ++j)
    {
        for (int i = 0; i < gridWidth; ++i)
        {
            float hL = getHeight(i - 1, j);
            float hR = getHeight(i + 1, j);
            float hD = getHeight(i, j - 1);
            float hU = getHeight(i, j + 1);
    
            glm::vec3 dX = glm::vec3(2.0f, hR - hL, 0.0f);
            glm::vec3 dZ = glm::vec3(0.0f, hU - hD, 2.0f);
    
            glm::vec3 normal = glm::normalize(glm::cross(dZ, dX));
    
            int vIdx = j * gridWidth + i;
            int base = vIdx * 6;
            vertices[base + 3] = normal.x;
            vertices[base + 4] = normal.y;
            vertices[base + 5] = normal.z;
        }
    }

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
        3,            
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),   // stride: 6 floats per vertex
        (void*)0
    );

    // normal attribute: layout(location = 1) in vec3 aNormal;
    glEnableVertexAttribArray(1);
    glVertexAttribPointer( // tell OpenGL how to read the data
        1,            // attribute location
        3,            
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),   // stride: 6 floats per vertex
        (void*)(3 * sizeof(float)) //offset: skip first 3 floats per vertex
    );

    glBindVertexArray(0); //unbind terrainVAO when not needed for best practice

    // ---------------- Shader & matrices ----------------

    Shader shader("shaders/vertex.glsl", "shaders/fragment.glsl",
                  "shaders/tess_control.glsl", "shaders/tess_eval.glsl");
    shader.use();

    // set patch size for tessellation (3 vertices = triangle)
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    // ---------------- Render loop ----------------
    while (!glfwWindowShouldClose(window))
    {
        // time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // toggle wireframe mode
        if (wireframeMode)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glClearColor(0.1f, 0.2f, 0.3f, 1.0f); //background color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use(); //sets the active shader program in OpenGL's state. glDrawElements knows to use this shader

        // recompute view & projection from camera
        glm::mat4 model = glm::mat4(1.0f);

        glm::mat4 view = glm::lookAt(
            cameraPos,
            cameraPos + cameraFront,
            cameraUp
        );

        glm::mat4 projection = glm::perspective(
            glm::radians(fov),
            800.0f / 600.0f,
            0.1f,
            100.0f
        );

        shader.setMat4("model", &model[0][0]);
        shader.setMat4("view", &view[0][0]);
        shader.setMat4("projection", &projection[0][0]);

        // set viewPos for distance-based LOD in tessellation and specular lighting
        shader.setVec3("viewPos", &cameraPos[0]);

        glBindVertexArray(terrainVAO); // bind again, so OpenGL knows which vertices and indices to use which we set earlier
        glDrawElements(GL_PATCHES,
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

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    float cameraSpeed = 3.0f * deltaTime; // units per second

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;

    // strafe: cross(front, up) gives right vector
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraRight;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraRight;

    // up / down
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;

    // toggle wireframe mode with T key
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
    {
        if (!tKeyPressed)
        {
            wireframeMode = !wireframeMode;
            tKeyPressed = true;
            std::cout << "Wireframe mode: " << (wireframeMode ? "ON" : "OFF") << "\n";
        }
    }
    else
    {
        tKeyPressed = false;
    }

    // close window with Esc
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos); // reversed: y goes down on screen
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    // clamp pitch
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}