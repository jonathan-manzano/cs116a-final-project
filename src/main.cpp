#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// ============================================================================
// GLOBAL SETTINGS
// ============================================================================

// Camera settings
glm::vec3 cameraPos   = glm::vec3(0.0f, 10.0f, 20.0f);  // Start position for 50x50 map
glm::vec3 cameraFront = glm::normalize(glm::vec3(0.0f, -0.3f, -1.0f));   // Look down slightly
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw   = -90.0f;
float pitch = -20.0f;
float fov   = 45.0f;

// Mouse input
float lastX = 400.0f;
float lastY = 300.0f;
bool firstMouse = true;

// Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Display toggles
bool wireframeMode = false;
bool tKeyPressed = false;

// Cursor control - click and hold to look around
bool cameraControlActive = false;

// Forward declaration for collision detection
struct TerrainMesh;

// Collision detection
const float CAMERA_HEIGHT_OFFSET = 0.15f;  // Height above terrain
TerrainMesh* g_terrainMesh = nullptr;       // Global access for collision

// Terrain settings
const int HEIGHTMAP_STEP = 5;        // Reduced from 8 for more detail
const float HEIGHT_SCALE = 3.0f;     // Increased for taller peaks

// Skybox cube vertices (36 vertices, 6 faces)
const float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window);
float getTerrainHeightAt(float worldX, float worldZ);

// ============================================================================
// TERRAIN MESH GENERATION
// ============================================================================

struct TerrainVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct TerrainMesh
{
    std::vector<TerrainVertex> vertices;
    std::vector<unsigned int> indices;
    int gridWidth;
    int gridHeight;
};

TerrainMesh generateTerrainMesh(const unsigned char* heightmapData, 
                                int imgWidth, int imgHeight, int step)
{
    TerrainMesh mesh;
    
    mesh.gridWidth = imgWidth / step;
    mesh.gridHeight = imgHeight / step;
    
    // Reserve space for vertices
    mesh.vertices.resize(mesh.gridWidth * mesh.gridHeight);
    
    // Generate vertex positions and UVs
    for (int j = 0; j < mesh.gridHeight; ++j)
    {
        for (int i = 0; i < mesh.gridWidth; ++i)
        {
            // Sample heightmap
            int imgX = i * step;
            int imgY = j * step;
            int imgIndex = imgY * imgWidth + imgX;
            unsigned char heightByte = heightmapData[imgIndex];
            float normalizedHeight = static_cast<float>(heightByte) / 255.0f;
            
            // Map grid position to 3D X, Z coordinates
            // World: -30 to 30 (60x60 world)
            float x = (static_cast<float>(i) / (mesh.gridWidth - 1)) * 60.0f - 30.0f;
            float z = (static_cast<float>(j) / (mesh.gridHeight - 1)) * 60.0f - 30.0f;
            float y = normalizedHeight * HEIGHT_SCALE;
            
            // Calculate UV coordinates (0 to 1)
            float u = static_cast<float>(i) / (mesh.gridWidth - 1);
            float v = static_cast<float>(j) / (mesh.gridHeight - 1);
            
            // Store vertex data
            int vertexIndex = j * mesh.gridWidth + i;
            mesh.vertices[vertexIndex].position = glm::vec3(x, y, z);
            mesh.vertices[vertexIndex].texCoord = glm::vec2(u, v);
        }
    }
    
    // Compute normals using height differences
    for (int j = 0; j < mesh.gridHeight; ++j)
    {
        for (int i = 0; i < mesh.gridWidth; ++i)
        {
            auto getHeight = [&](int gi, int gj) -> float {
                gi = std::max(0, std::min(gi, mesh.gridWidth - 1));
                gj = std::max(0, std::min(gj, mesh.gridHeight - 1));
                return mesh.vertices[gj * mesh.gridWidth + gi].position.y;
            };
            
            float hLeft  = getHeight(i - 1, j);
            float hRight = getHeight(i + 1, j);
            float hDown  = getHeight(i, j - 1);
            float hUp    = getHeight(i, j + 1);
            
            glm::vec3 tangentX = glm::vec3(2.0f, hRight - hLeft, 0.0f);
            glm::vec3 tangentZ = glm::vec3(0.0f, hUp - hDown, 2.0f);
            glm::vec3 normal = glm::normalize(glm::cross(tangentZ, tangentX));
            
            int vertexIndex = j * mesh.gridWidth + i;
            mesh.vertices[vertexIndex].normal = normal;
        }
    }
    
    // Generate indices for triangle mesh
    mesh.indices.reserve((mesh.gridWidth - 1) * (mesh.gridHeight - 1) * 6);
    
    for (int j = 0; j < mesh.gridHeight - 1; ++j)
    {
        for (int i = 0; i < mesh.gridWidth - 1; ++i)
        {
            unsigned int topLeft     = j * mesh.gridWidth + i;
            unsigned int topRight    = j * mesh.gridWidth + (i + 1);
            unsigned int bottomLeft  = (j + 1) * mesh.gridWidth + i;
            unsigned int bottomRight = (j + 1) * mesh.gridWidth + (i + 1);
            
            // First triangle (top-left, bottom-left, top-right)
            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);
            
            // Second triangle (top-right, bottom-left, bottom-right)
            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }
    
    return mesh;
}

// ============================================================================
// COLLISION DETECTION
// ============================================================================

float getTerrainHeightAt(float worldX, float worldZ)
{
    if (!g_terrainMesh) return 0.0f;
    
    // Convert world coordinates back to grid coordinates
    // World space: -30 to 30, Grid space: 0 to gridWidth-1
    float gridX = (worldX + 30.0f) / 60.0f * (g_terrainMesh->gridWidth - 1);
    float gridZ = (worldZ + 30.0f) / 60.0f * (g_terrainMesh->gridHeight - 1);
    
    // Clamp to valid grid range
    if (gridX < 0 || gridX >= g_terrainMesh->gridWidth - 1 ||
        gridZ < 0 || gridZ >= g_terrainMesh->gridHeight - 1)
    {
        return 0.0f;  // Outside terrain bounds
    }
    
    // Get integer grid coordinates
    int x0 = static_cast<int>(gridX);
    int z0 = static_cast<int>(gridZ);
    int x1 = x0 + 1;
    int z1 = z0 + 1;
    
    // Get fractional part for interpolation
    float fx = gridX - x0;
    float fz = gridZ - z0;
    
    // Get heights at four corners of grid cell
    float h00 = g_terrainMesh->vertices[z0 * g_terrainMesh->gridWidth + x0].position.y;
    float h10 = g_terrainMesh->vertices[z0 * g_terrainMesh->gridWidth + x1].position.y;
    float h01 = g_terrainMesh->vertices[z1 * g_terrainMesh->gridWidth + x0].position.y;
    float h11 = g_terrainMesh->vertices[z1 * g_terrainMesh->gridWidth + x1].position.y;
    
    // Bilinear interpolation
    float h0 = h00 * (1 - fx) + h10 * fx;
    float h1 = h01 * (1 - fx) + h11 * fx;
    float height = h0 * (1 - fz) + h1 * fz;
    
    return height;
}

// ============================================================================
// MAIN PROGRAM
// ============================================================================

int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // Configure OpenGL version (4.1 for macOS)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Terrain Renderer", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // Configure OpenGL
    glEnable(GL_DEPTH_TEST);
    
    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    
    // Cursor starts free (normal mode)
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // ========================================================================
    // LOAD HEIGHTMAP
    // ========================================================================
    
    const char* heightmapPath = "assets/heightmapper-1764410934226.png";
    int imgWidth = 0, imgHeight = 0, imgChannels = 0;
    
    unsigned char* imageData = stbi_load(heightmapPath, &imgWidth, &imgHeight, 
                                         &imgChannels, 1);
    
    if (!imageData)
    {
        std::cerr << "Failed to load heightmap: " << heightmapPath << "\n";
        std::cerr << "Reason: " << stbi_failure_reason() << "\n";
        glfwTerminate();
        return -1;
    }
    
    std::cout << "Loaded heightmap: " << heightmapPath << "\n";
    std::cout << "  Size: " << imgWidth << " x " << imgHeight << "\n";
    std::cout << "  Channels: 1 (grayscale)\n";

    // ========================================================================
    // GENERATE TERRAIN MESH
    // ========================================================================
    
    TerrainMesh terrain = generateTerrainMesh(imageData, imgWidth, imgHeight, 
                                              HEIGHTMAP_STEP);
    stbi_image_free(imageData);
    
    // Set global pointer for collision detection
    g_terrainMesh = &terrain;
    
    std::cout << "Generated terrain mesh:\n";
    std::cout << "  Vertices: " << terrain.vertices.size() << "\n";
    std::cout << "  Triangles: " << terrain.indices.size() / 3 << "\n";
    std::cout << "  Grid size: " << terrain.gridWidth << " x " << terrain.gridHeight << "\n";

    // ========================================================================
    // SETUP OPENGL BUFFERS
    // ========================================================================
    
    unsigned int terrainVAO, terrainVBO, terrainEBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glGenBuffers(1, &terrainEBO);
    
    glBindVertexArray(terrainVAO);
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 terrain.vertices.size() * sizeof(TerrainVertex),
                 terrain.vertices.data(), 
                 GL_STATIC_DRAW);
    
    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 terrain.indices.size() * sizeof(unsigned int),
                 terrain.indices.data(),
                 GL_STATIC_DRAW);
    
    // Position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)0);
    
    // Normal attribute (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), 
                        (void*)offsetof(TerrainVertex, normal));
    
    // UV attribute (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), 
                        (void*)offsetof(TerrainVertex, texCoord));
    
    glBindVertexArray(0);

    // ========================================================================
    // SETUP SKYBOX
    // ========================================================================
    
    // ========================================================================
    // SETUP SKYBOX
    // ========================================================================
    
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    
    // Position attribute for skybox
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glBindVertexArray(0);
    
    std::cout << "Skybox initialized\n";

    // ========================================================================
    // LOAD SHADERS
    // ========================================================================
    
    Shader terrainShader("shaders/vertex.glsl", "shaders/fragment.glsl",
                         "shaders/tess_control.glsl", "shaders/tess_eval.glsl");
    
    Shader skyboxShader("shaders/skybox_vertex.glsl", "shaders/skybox_fragment.glsl");
    
    if (!terrainShader.isValid() || !skyboxShader.isValid())
    {
        std::cerr << "ERROR: Failed to load shaders. Check shaders/ directory.\n";
        glfwTerminate();
        return -1;
    }
    
    std::cout << "Shaders loaded successfully\n";
    
    // Set tessellation patch size
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    // ========================================================================
    // RENDER LOOP
    // ========================================================================
    
    while (!glfwWindowShouldClose(window))
    {
        // Update time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Process input
        processInput(window);

        // Toggle wireframe mode
        glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);

        // Clear buffers
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Setup matrices (used by both skybox and terrain)
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 
                                                0.1f, 180.0f);  // Far plane for 60x60 map

        // ===== RENDER SKYBOX =====
        glDepthFunc(GL_LEQUAL); // Change depth function for skybox
        skyboxShader.use();
        skyboxShader.setMat4("view", &view[0][0]);
        skyboxShader.setMat4("projection", &projection[0][0]);
        
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // Restore default depth function

        // ===== RENDER TERRAIN =====
        terrainShader.use();
        
        glm::mat4 model = glm::mat4(1.0f);
        terrainShader.setMat4("model", &model[0][0]);
        terrainShader.setMat4("view", &view[0][0]);
        terrainShader.setMat4("projection", &projection[0][0]);
        terrainShader.setVec3("viewPos", &cameraPos[0]);

        // Draw terrain
        glBindVertexArray(terrainVAO);
        glDrawElements(GL_PATCHES, static_cast<GLsizei>(terrain.indices.size()),
                       GL_UNSIGNED_INT, (void*)0);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &terrainEBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    
    glfwTerminate();
    return 0;
}

// ============================================================================
// CALLBACK FUNCTIONS
// ============================================================================

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    float cameraSpeed = 20.0f * deltaTime;  // Faster speed for 50x50 map

    // Test each movement direction separately for collision
    glm::vec3 proposedPos = cameraPos;

    // Forward/Back movement (W/S)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        proposedPos = cameraPos + cameraSpeed * cameraFront;
        float terrainHeight = getTerrainHeightAt(proposedPos.x, proposedPos.z);
        if (proposedPos.y >= terrainHeight + CAMERA_HEIGHT_OFFSET)
            cameraPos = proposedPos;  // Accept movement
        else
            cameraPos.y = terrainHeight + CAMERA_HEIGHT_OFFSET;  // Clamp Y only
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        proposedPos = cameraPos - cameraSpeed * cameraFront;
        float terrainHeight = getTerrainHeightAt(proposedPos.x, proposedPos.z);
        if (proposedPos.y >= terrainHeight + CAMERA_HEIGHT_OFFSET)
            cameraPos = proposedPos;  // Accept movement
        else
            cameraPos.y = terrainHeight + CAMERA_HEIGHT_OFFSET;  // Clamp Y only
    }

    // Left/Right movement (A/D)
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        proposedPos = cameraPos - cameraSpeed * cameraRight;
        float terrainHeight = getTerrainHeightAt(proposedPos.x, proposedPos.z);
        if (proposedPos.y >= terrainHeight + CAMERA_HEIGHT_OFFSET)
            cameraPos = proposedPos;  // Accept movement
        else
            cameraPos.y = terrainHeight + CAMERA_HEIGHT_OFFSET;  // Clamp Y only
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        proposedPos = cameraPos + cameraSpeed * cameraRight;
        float terrainHeight = getTerrainHeightAt(proposedPos.x, proposedPos.z);
        if (proposedPos.y >= terrainHeight + CAMERA_HEIGHT_OFFSET)
            cameraPos = proposedPos;  // Accept movement
        else
            cameraPos.y = terrainHeight + CAMERA_HEIGHT_OFFSET;  // Clamp Y only
    }

    // Vertical movement (Space/Ctrl) - no horizontal collision check needed
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos.y += cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    {
        cameraPos.y -= cameraSpeed;
        // Still can't go below terrain
        float terrainHeight = getTerrainHeightAt(cameraPos.x, cameraPos.z);
        if (cameraPos.y < terrainHeight + CAMERA_HEIGHT_OFFSET)
            cameraPos.y = terrainHeight + CAMERA_HEIGHT_OFFSET;
    }

    // Final safety check - ensure we're always above terrain
    float finalTerrainHeight = getTerrainHeightAt(cameraPos.x, cameraPos.z);
    if (cameraPos.y < finalTerrainHeight + CAMERA_HEIGHT_OFFSET)
        cameraPos.y = finalTerrainHeight + CAMERA_HEIGHT_OFFSET;

    // Toggle wireframe mode (T key)
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

    // Exit (Escape key)
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // Only process mouse movement when camera control is active (mouse held down)
    if (!cameraControlActive)
    {
        firstMouse = true;
        return;
    }

    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos);
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Clamp pitch
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // Calculate new camera front vector
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    // Left mouse button controls camera
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            // Start camera control
            cameraControlActive = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (action == GLFW_RELEASE)
        {
            // Stop camera control
            cameraControlActive = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            firstMouse = true; // Reset for next time
        }
    }
}