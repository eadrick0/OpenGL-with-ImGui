#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "glad/include/glad/glad.h"
#include <GLFW/glfw3.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstddef>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const unsigned int SCR_WIDTH = 1366;
const unsigned int SCR_HEIGHT = 768;
const char* WINDOW_TITLE = "OpenGL";
const char* GLSL_VERSION = "#version 330 core";
const char* SETTINGS_FILENAME = "app_settings.ini";

struct Vertex {
    float position[3];
    float color[3];
    float texCoords[2];
};

enum class ShapeType {
    NONE,
    TRIANGLE,
    QUAD,
    CIRCLE
};

GLFWwindow* window = nullptr;
ShapeType currentShape = ShapeType::NONE;
bool wireframeMode = false;
float shapeColor[3] = { 1.0f, 1.0f, 0.0f };
bool useUniformColor = true;
float translation[2] = { 0.0f, 0.0f };
float rotationAngle = 0.0f;
float scale = 1.0f;
float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
bool showMenu = true;

GLuint triangleVAO = 0, triangleVBO = 0;
GLuint quadVAO = 0, quadVBO = 0, quadEBO = 0;
GLsizei quadIndexCount = 0;
GLuint circleVAO = 0, circleVBO = 0, circleEBO = 0;
GLsizei circleIndexCount = 0;
int circleSegments = 36;

GLuint shaderProgram = 0;
GLint modelLoc = -1, viewLoc = -1, projLoc = -1;
GLint overrideColorLoc = -1, useOverrideColorLoc = -1;
GLint useTextureLoc = -1;

GLuint textureID = 0;
bool enableTexture = false;

glm::vec2 cameraOffset = glm::vec2(0.0f, 0.0f);
float cameraZoom = 1.0f;
bool isDragging = false;
double lastMouseX = 0.0, lastMouseY = 0.0;

void checkCompileErrors(GLuint shader, std::string type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}

bool setupShaders() {
    const char* vertexShaderSource = R"glsl(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;
        layout (location = 2) in vec2 aTexCoord;
        out vec3 vertexColor;
        out vec2 TexCoord;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform vec3 overrideColor;
        uniform bool useOverrideColor;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            if (useOverrideColor) {
                vertexColor = overrideColor;
            } else {
                vertexColor = aColor;
            }
            TexCoord = aTexCoord;
        }
    )glsl";
    const char* fragmentShaderSource = R"glsl(
        #version 330 core
        out vec4 FragColor;
        in vec3 vertexColor;
        in vec2 TexCoord;
        uniform sampler2D ourTexture;
        uniform bool useTexture;
        void main() {
            vec4 texColor = texture(ourTexture, TexCoord);
            vec4 finalColor = vec4(vertexColor, 1.0);
            if (useTexture) {
                 finalColor = vec4(vertexColor, 1.0) * texColor;
            }
             FragColor = finalColor;
        }
    )glsl";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkCompileErrors(vertexShader, "VERTEX");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkCompileErrors(fragmentShader, "FRAGMENT");

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkCompileErrors(shaderProgram, "PROGRAM");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linkSuccess;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkSuccess);
    if (!linkSuccess) {
        shaderProgram = 0;
        return false;
    }
    std::cout << "Shader program created and linked successfully." << std::endl;
    return true;
}

bool getUniformLocations() {
    if (shaderProgram == 0) return false;
    modelLoc = glGetUniformLocation(shaderProgram, "model");
    viewLoc = glGetUniformLocation(shaderProgram, "view");
    projLoc = glGetUniformLocation(shaderProgram, "projection");
    overrideColorLoc = glGetUniformLocation(shaderProgram, "overrideColor");
    useOverrideColorLoc = glGetUniformLocation(shaderProgram, "useOverrideColor");
    useTextureLoc = glGetUniformLocation(shaderProgram, "useTexture");

    if (modelLoc == -1 || viewLoc == -1 || projLoc == -1 || overrideColorLoc == -1 || useOverrideColorLoc == -1 || useTextureLoc == -1) {
        std::cerr << "Warning: Failed to get all uniform locations! Check shader code and names." << std::endl;
    }
    return true;
}

GLuint loadTexture(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;
        else {
            std::cerr << "Unsupported texture format: Number of Components = " << nrComponents << std::endl;
            stbi_image_free(data);
            glDeleteTextures(1, &textureID);
            return 0;
        }
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
        std::cout << "Texture loaded successfully: " << path << std::endl;
    }
    else {
        std::cerr << "Failed to load texture: " << path << std::endl;
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

void setupTriangle() {
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}}
    };
    glGenVertexArrays(1, &triangleVAO); glGenBuffers(1, &triangleVBO);
    glBindVertexArray(triangleVAO); glBindBuffer(GL_ARRAY_BUFFER, triangleVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position)); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords)); glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
}

void setupQuad() {
    std::vector<Vertex> vertices = {
        {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
    };
    std::vector<unsigned int> indices = { 0, 1, 3, 1, 2, 3 };
    quadIndexCount = static_cast<GLsizei>(indices.size());
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO); glGenBuffers(1, &quadEBO);
    glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position)); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords)); glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
}

void setupCircle(int numSegments) {
    if (circleVAO == 0) { glGenVertexArrays(1, &circleVAO); glGenBuffers(1, &circleVBO); glGenBuffers(1, &circleEBO); }
    std::vector<Vertex> vertices; std::vector<unsigned int> indices;
    float radius = 0.5f;
    vertices.push_back({ {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f} });
    for (int i = 0; i <= numSegments; ++i) {
        float angle = 2.0f * M_PI * float(i) / float(numSegments);
        float x = radius * cosf(angle); float y = radius * sinf(angle);
        float r = (cosf(angle) + 1.0f) * 0.5f; float g = (sinf(angle) + 1.0f) * 0.5f;
        float u = (x / radius + 1.0f) * 0.5f; float v = (y / radius + 1.0f) * 0.5f;
        vertices.push_back({ {x, y, 0.0f}, {r, g, 0.5f}, {u, v} });
    }
    for (int i = 0; i < numSegments; ++i) { indices.push_back(0); indices.push_back(i + 1); indices.push_back(i + 2); }
    circleIndexCount = static_cast<GLsizei>(indices.size());
    glBindVertexArray(circleVAO); glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, circleEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position)); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords)); glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
}

void saveSettings() {
    std::ofstream outFile(SETTINGS_FILENAME);
    if (!outFile) { std::cerr << "Failed to write settings file: " << SETTINGS_FILENAME << std::endl; return; }
    outFile << "Shape " << static_cast<int>(currentShape) << std::endl;
    outFile << "Wireframe " << wireframeMode << std::endl;
    outFile << "UseUniformColor " << useUniformColor << std::endl;
    outFile << "ShapeColor " << shapeColor[0] << " " << shapeColor[1] << " " << shapeColor[2] << std::endl;
    outFile << "ClearColor " << clearColor[0] << " " << clearColor[1] << " " << clearColor[2] << " " << clearColor[3] << std::endl;
    outFile << "Translation " << translation[0] << " " << translation[1] << std::endl;
    outFile << "Rotation " << rotationAngle << std::endl;
    outFile << "Scale " << scale << std::endl;
    outFile << "CircleSegments " << circleSegments << std::endl;
    outFile << "EnableTexture " << enableTexture << std::endl;
    outFile << "CameraOffset " << cameraOffset.x << " " << cameraOffset.y << std::endl;
    outFile << "CameraZoom " << cameraZoom << std::endl;
    outFile << "ShowMenu " << showMenu << std::endl;
    std::cout << "Settings saved: " << SETTINGS_FILENAME << std::endl;
}

void loadSettings() {
    std::ifstream inFile(SETTINGS_FILENAME);
    if (!inFile) { std::cerr << "Settings file not found or could not be read: " << SETTINGS_FILENAME << std::endl; return; }
    std::string line;
    int loadedSegments = circleSegments;
    while (std::getline(inFile, line)) {
        std::stringstream ss(line); std::string key; ss >> key;
        if (key == "Shape") { int i; ss >> i; currentShape = static_cast<ShapeType>(i); }
        else if (key == "Wireframe") ss >> wireframeMode;
        else if (key == "UseUniformColor") ss >> useUniformColor;
        else if (key == "ShapeColor") ss >> shapeColor[0] >> shapeColor[1] >> shapeColor[2];
        else if (key == "ClearColor") ss >> clearColor[0] >> clearColor[1] >> clearColor[2] >> clearColor[3];
        else if (key == "Translation") ss >> translation[0] >> translation[1];
        else if (key == "Rotation") ss >> rotationAngle;
        else if (key == "Scale") ss >> scale;
        else if (key == "CircleSegments") ss >> loadedSegments;
        else if (key == "EnableTexture") ss >> enableTexture;
        else if (key == "CameraOffset") ss >> cameraOffset.x >> cameraOffset.y;
        else if (key == "CameraZoom") ss >> cameraZoom;
        else if (key == "ShowMenu") ss >> showMenu;
    }
    if (loadedSegments != circleSegments) {
        circleSegments = loadedSegments;
        setupCircle(circleSegments);
    }
    std::cout << "Settings loaded: " << SETTINGS_FILENAME << std::endl;
}

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
        isDragging = true; glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
    }
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE) {
        isDragging = false;
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (isDragging) {
        float dx = static_cast<float>(xpos - lastMouseX); float dy = static_cast<float>(ypos - lastMouseY);
        int width, height; glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            cameraOffset.x += dx * (2.0f / width) * (1.0f / cameraZoom);
            cameraOffset.y -= dy * (2.0f / height) * (1.0f / cameraZoom);
        }
        lastMouseX = xpos; lastMouseY = ypos;
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    if (ImGui::GetIO().WantCaptureMouse) return;
    float zoomSensitivity = 0.1f;
    cameraZoom += static_cast<float>(yoffset) * zoomSensitivity * cameraZoom;
    if (cameraZoom < 0.05f) cameraZoom = 0.05f;
    if (cameraZoom > 20.0f) cameraZoom = 20.0f;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    if (key == GLFW_KEY_INSERT && action == GLFW_PRESS) {
        showMenu = !showMenu;
    }
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int main() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window); glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { return -1; }
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);

    static const ImWchar font_ranges[] = { 0x0020, 0x00FF, 0x0100, 0x017F, 0x00C7,0x00C7, 0x00E7,0x00E7, 0x00D6,0x00D6, 0x00F6,0x00F6, 0x00DC,0x00DC, 0x00FC,0x00FC, 0, };
    const char* font_path = "C:/Windows/Fonts/Arial.ttf";
    float font_size = 15.0f;
    ImFont* font = io.Fonts->AddFontFromFileTTF(font_path, font_size, nullptr, font_ranges);
    if (!font) { std::cerr << "Warning: Failed to load font! -> " << font_path << std::endl; io.Fonts->AddFontDefault(); }
    else { std::cout << "Font loaded successfully: " << font_path << std::endl; }

    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);

    if (!setupShaders() || !getUniformLocations()) return -1;

    setupTriangle();
    setupQuad();
    setupCircle(circleSegments);
    textureID = loadTexture("container.jpg");

    loadSettings();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (showMenu) {
            ImGui::Begin("Control Panel");
            if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) { ImGui::Text("Avg. %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate); }
            if (ImGui::CollapsingHeader("Shape Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::RadioButton("None", currentShape == ShapeType::NONE)) { currentShape = ShapeType::NONE; } ImGui::SameLine();
                if (ImGui::RadioButton("Triangle", currentShape == ShapeType::TRIANGLE)) { currentShape = ShapeType::TRIANGLE; } ImGui::SameLine();
                if (ImGui::RadioButton("Quad", currentShape == ShapeType::QUAD)) { currentShape = ShapeType::QUAD; } ImGui::SameLine();
                if (ImGui::RadioButton("Circle", currentShape == ShapeType::CIRCLE)) { currentShape = ShapeType::CIRCLE; }
                if (currentShape == ShapeType::CIRCLE) {
                    ImGui::SameLine(); ImGui::Text(" | "); ImGui::SameLine(); ImGui::SetNextItemWidth(100);
                    int segments = circleSegments;
                    if (ImGui::SliderInt("Segments", &segments, 3, 100)) {
                        if (segments != circleSegments) {
                            circleSegments = segments;
                            setupCircle(circleSegments);
                        }
                    }
                }
            }
            if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Wireframe Mode", &wireframeMode);
                ImGui::Checkbox("Use Picker Color", &useUniformColor); ImGui::SameLine(); ImGui::ColorEdit3("Shape Color", shapeColor);
                ImGui::ColorEdit4("Background", clearColor);
                ImGui::Checkbox("Use Texture (Quad)", &enableTexture);
                if (textureID == 0 && enableTexture) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1, 0, 0, 1), " (Texture failed to load!)"); }
            }
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat2("Position", translation, 0.01f);
                ImGui::SliderAngle("Rotation (Z)", &rotationAngle, -180.0f, 180.0f);
                ImGui::DragFloat("Scale", &scale, 0.02f, 0.05f, 20.0f);
                if (ImGui::Button("Reset Transform")) { translation[0] = 0; translation[1] = 0; rotationAngle = 0; scale = 1; }
            }
            if (ImGui::CollapsingHeader("Settings")) {
                if (ImGui::Button("Save Settings")) { saveSettings(); } ImGui::SameLine();
                if (ImGui::Button("Load Settings")) { loadSettings(); }
            }
            ImGui::End();
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);

        if (currentShape != ShapeType::NONE && shaderProgram != 0) {
            glUseProgram(shaderProgram);
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(translation[0], translation[1], 0.0f));
            model = glm::rotate(model, glm::radians(rotationAngle), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(scale, scale, scale));
            if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-cameraOffset.x, -cameraOffset.y, 0.0f));
            if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

            float aspectRatio = (display_h > 0) ? static_cast<float>(display_w) / display_h : 1.0f;
            float orthoWidth = aspectRatio / cameraZoom; float orthoHeight = 1.0f / cameraZoom;
            glm::mat4 projection = glm::ortho(-orthoWidth, orthoWidth, -orthoHeight, orthoHeight, -1.0f, 1.0f);
            if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

            if (overrideColorLoc != -1) glUniform3fv(overrideColorLoc, 1, shapeColor);
            if (useOverrideColorLoc != -1) glUniform1i(useOverrideColorLoc, useUniformColor ? 1 : 0);

            bool actuallyUseTexture = enableTexture && (currentShape == ShapeType::QUAD) && (textureID != 0);
            if (useTextureLoc != -1) glUniform1i(useTextureLoc, actuallyUseTexture ? 1 : 0);
            if (actuallyUseTexture) {
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, textureID);
                glUniform1i(glGetUniformLocation(shaderProgram, "ourTexture"), 0);
            }

            switch (currentShape) {
            case ShapeType::TRIANGLE: glBindVertexArray(triangleVAO); glDrawArrays(GL_TRIANGLES, 0, 3); break;
            case ShapeType::QUAD:     glBindVertexArray(quadVAO); glDrawElements(GL_TRIANGLES, quadIndexCount, GL_UNSIGNED_INT, 0); break;
            case ShapeType::CIRCLE:   glBindVertexArray(circleVAO); glDrawElements(GL_TRIANGLES, circleIndexCount, GL_UNSIGNED_INT, 0); break;
            default: break;
            }
            glBindVertexArray(0);
            if (actuallyUseTexture) { glBindTexture(GL_TEXTURE_2D, 0); }
            glUseProgram(0);
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    std::cout << "Cleaning up..." << std::endl;
    saveSettings();

    glDeleteTextures(1, &textureID);
    glDeleteVertexArrays(1, &triangleVAO); glDeleteBuffers(1, &triangleVBO);
    glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO); glDeleteBuffers(1, &quadEBO);
    glDeleteVertexArrays(1, &circleVAO); glDeleteBuffers(1, &circleVBO); glDeleteBuffers(1, &circleEBO);
    if (shaderProgram != 0) {
        glDeleteProgram(shaderProgram);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "Program terminated." << std::endl;

    return 0;
}