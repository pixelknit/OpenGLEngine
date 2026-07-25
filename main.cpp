#include "camera.h"
#include "scene_manager.h"
#include "shader.h"
#include "test_callback.h"
#include "ui.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
// #include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

Camera camera(glm::vec3(0.0f, 2.0f, 10.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Walk / fly toggle
bool flyMode = false;
bool qPreviouslyPressed = false;

// Editor mode: cursor released, camera frozen, panels clickable (Tab toggles)
bool uiMode = false;
bool tabPreviouslyPressed = false;

// Run state (updated each frame by processInput, consumed by UpdateBob)
bool isMoving = false;
bool isRunning = false;
const float RUN_SPEED_MULT = 2.2f;

// Walk-mode physics
float verticalVelocity = 0.0f;
bool isOnGround = false;
const float GRAVITY = -20.0f;
const float JUMP_FORCE = 7.0f;
const float PLAYER_EYE_HEIGHT = 3.5f;

// Shadow map dimensions
const unsigned int shadow_dim{2048};
const unsigned int SHADOW_WIDTH = shadow_dim, SHADOW_HEIGHT = shadow_dim;
unsigned int depthMapFBO;
unsigned int depthMap;

// Scene FBO — color + normals + albedo (MRT) + depth
unsigned int sceneFBO;
unsigned int sceneColorTex, sceneNormalTex, sceneDepthTex, sceneAlbedoTex;

// SSGI FBO — indirect diffuse result
unsigned int ssgiFBO;
unsigned int ssgiColorTex;

// RSM data textures (color attachments on depthMapFBO, shadow-map resolution)
unsigned int rsmPosTex, rsmNormalTex, rsmFluxTex;

// RSM indirect lighting result (screen resolution)
unsigned int rsmIndirectFBO;
unsigned int rsmIndirectTex;

// Fullscreen quad for SSR composite pass
unsigned int quadVAO, quadVBO;
float quadVertices[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

unsigned int loadEquirectangularMap(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    float *data = stbi_loadf(path, &width, &height, &nrComponents, 0);
    
    if (data) {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        stbi_image_free(data);
    } else {
        std::cout << "Failed to load HDR image: " << path << std::endl;
        stbi_image_free(data);
    }
    return textureID;
}

// HDRIs stay resident once loaded, so switching back to one already picked in
// the editor dropdown doesn't re-decode the file off disk.
std::unordered_map<std::string, unsigned int> envMapCache;

unsigned int getEnvMap(const std::string &path) {
    auto it = envMapCache.find(path);
    if (it != envMapCache.end())
        return it->second;

    unsigned int id = loadEquirectangularMap(path.c_str());
    envMapCache[path] = id;
    return id;
}

// Helper to render a skybox
float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    // ... (standard cube, 36 vertices total)
    // Full array:
    -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f
};

unsigned int skyboxVAO, skyboxVBO;

void initSkybox() {
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

void initSceneFBO() {
    glGenFramebuffers(1, &sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

    // Attachment 0: linear HDR scene color
    glGenTextures(1, &sceneColorTex);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTex, 0);

    // Attachment 1: view-space normals (encoded) + roughness in alpha
    glGenTextures(1, &sceneNormalTex);
    glBindTexture(GL_TEXTURE_2D, sceneNormalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, sceneNormalTex, 0);

    // Attachment 2: linear albedo RGB + metallic A (for SSGI color bleeding)
    glGenTextures(1, &sceneAlbedoTex);
    glBindTexture(GL_TEXTURE_2D, sceneAlbedoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, sceneAlbedoTex, 0);

    // Depth texture (must be sampleable for SSR/SSGI position reconstruction)
    glGenTextures(1, &sceneDepthTex);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SCR_WIDTH, SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepthTex, 0);

    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Scene FBO incomplete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initQuad() {
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void initRSMIndirectFBO() {
    glGenFramebuffers(1, &rsmIndirectFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, rsmIndirectFBO);

    glGenTextures(1, &rsmIndirectTex);
    glBindTexture(GL_TEXTURE_2D, rsmIndirectTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rsmIndirectTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "RSM indirect FBO incomplete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initSSGIFBO() {
    glGenFramebuffers(1, &ssgiFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssgiFBO);

    glGenTextures(1, &ssgiColorTex);
    glBindTexture(GL_TEXTURE_2D, ssgiColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssgiColorTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSGI FBO incomplete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderSkybox(Shader &skyboxShader, unsigned int envMap) {
    glDepthFunc(GL_LEQUAL);
    skyboxShader.use();
    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, envMap);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

int main() {
  TestCallback test1 = TestCallback();
  test1.PrintTest();
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RETINAL ENGINE", NULL, NULL);
  if (window == NULL) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  // After our own GLFW callbacks are installed: the ImGui backend chains onto
  // them rather than replacing them.
  EditorUI::Init(window);

  glEnable(GL_DEPTH_TEST);
  // glEnable(GL_CULL_FACE);

  // Build and compile shaders
  Shader pbrShader("shaders/pbr.vs", "shaders/pbr.fs");
  Shader simpleDepthShader("shaders/shadow_depth.vs", "shaders/shadow_depth.fs");
  Shader rsmDepthShader("shaders/rsm_depth.vs", "shaders/rsm_depth.fs");
  Shader rsmIndirectShader("shaders/ssr.vs", "shaders/rsm_indirect.fs");
  Shader ssrShader("shaders/ssr.vs", "shaders/ssr.fs");
  Shader ssgiShader("shaders/ssr.vs", "shaders/ssgi.fs");

  // Scene objects: each AddEntity call loads the OBJ, loads its PBR texture
  Scene scene;
  scene.AddEntity("ground", "models/plane/simple_plane.obj", "models/plane",
                   Transform{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.8f)});
  scene.AddEntity("cup", "models/cup/cup.obj", "models/cup",
                   Transform{glm::vec3(1.0f, 2.05f, 0.0f), glm::vec3(0.0f), glm::vec3(0.5f)});
  scene.AddEntity("table", "models/table/table.obj", "models/table",
                   Transform{glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.02f)});
  scene.AddEntityLOD("rock", 
                  {"models/coast_rock/coast_rock.obj",
                  "models/coast_rock/coast_rock_LOD1.obj",
                  "models/coast_rock/coast_rock_LOD2.obj"}, 
                  {20.0f, 45.0f} ,"models/coast_rock",
                   Transform{glm::vec3(12.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f)});
  scene.AddScatteredEntityLOD("trench_rock",
                      {"models/ground_trench_rock01/trench_rock01.obj",
                       "models/ground_trench_rock01/trench_rock01_LOD1.obj",
                       "models/ground_trench_rock01/trench_rock01_LOD2.obj"},
                      {20.0f, 45.0f}, "models/ground_trench_rock01",
                      /*count*/ 40, /*center*/ glm::vec3(1.5f, 0.0f, 0.0f), /*radius*/ 12.0f,
                      /*scaleRange*/ glm::vec2(6.0f, 14.0f));

  /////////env map///////
  Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");
  initSkybox();
  initSceneFBO();
  initQuad();
  initRSMIndirectFBO();
  initSSGIFBO();

  // Editor state — every panel value lives here; the render loop reads it back
  // when filling in uniforms.
  EditorState editor;
  editor.hdriPaths = EditorUI::ScanHDRIs("models");
  for (size_t i = 0; i < editor.hdriPaths.size(); ++i)
    if (editor.hdriPaths[i] == "models/env_map.hdr")
      editor.hdriIndex = (int)i;

  // Load environment map. If the scan turned up nothing (assets moved?), fall
  // back to the original hardcoded path so the skybox still has something.
  unsigned int envMap = editor.hdriPaths.empty()
                            ? loadEquirectangularMap("models/env_map.hdr")
                            : getEnvMap(editor.hdriPaths[editor.hdriIndex]);
  int loadedHdriIndex = editor.hdriIndex;

  // Configure depth map FBO
  glGenFramebuffers(1, &depthMapFBO);
  glGenTextures(1, &depthMap);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH,
               SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

  float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depthMap, 0);

  // RSM color attachments — world position, normal, flux captured during shadow pass
  auto makeRSMTex = [](unsigned int &id) {
      glGenTextures(1, &id);
      glBindTexture(GL_TEXTURE_2D, id);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, shadow_dim, shadow_dim,
                   0, GL_RGB, GL_FLOAT, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  };
  makeRSMTex(rsmPosTex);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rsmPosTex, 0);
  makeRSMTex(rsmNormalTex);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, rsmNormalTex, 0);
  makeRSMTex(rsmFluxTex);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, rsmFluxTex, 0);

  // Keep default draw state as "none" — the render loop sets it explicitly each frame
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Directional sun light. Direction, intensity and tint are driven by the
  // editor's Sun panel and recomputed each frame; the shadow camera looks at
  // shadowTarget from whichever direction the sun currently points.
  const glm::vec3 shadowTarget(6.0f, 0.0f, 0.0f);

  // Remaining point lights
  glm::vec3 lightColors[] = {glm::vec3(100.0f, 100.0f, 100.0f),
                             glm::vec3(100.0f, 100.0f, 100.0f)};
  glm::vec3 lightPositions[] = {glm::vec3(10.0f, -10.0f, 10.0f),
                                glm::vec3(-10.0f, 10.0f, 10.0f)};

  
  // Configure PBR shader
  pbrShader.use();
  pbrShader.setInt("albedoMap", 0);
  pbrShader.setInt("normalMap", 1);
  pbrShader.setInt("metallicMap", 2);
  pbrShader.setInt("roughnessMap", 3);
  pbrShader.setInt("aoMap", 4);
  pbrShader.setInt("shadowMap", 5); // Shadow map in here :)
  pbrShader.setInt("envMap", 6);
  // envMapIntensity / minRoughness / metallicMult are set per frame from the
  // editor's Rendering panel.

  skyboxShader.use();
  skyboxShader.setInt("envMap", 0);

  rsmDepthShader.use();
  rsmDepthShader.setInt("albedoMap", 0);

  rsmIndirectShader.use();
  rsmIndirectShader.setInt("sceneNormalTex", 0);
  rsmIndirectShader.setInt("sceneDepthTex",  1);
  rsmIndirectShader.setInt("rsmPosTex",      2);
  rsmIndirectShader.setInt("rsmNormalTex",   3);
  rsmIndirectShader.setInt("rsmFluxTex",     4);

  ssrShader.use();
  ssrShader.setInt("sceneTex", 0);
  ssrShader.setInt("normalTex", 1);
  ssrShader.setInt("depthTex",  2);
  ssrShader.setInt("ssgiTex",   3);
  ssrShader.setInt("rsmTex",    4);

  ssgiShader.use();
  ssgiShader.setInt("sceneTex",  0);
  ssgiShader.setInt("normalTex", 1);
  ssgiShader.setInt("depthTex",  2);
  ssgiShader.setInt("albedoTex", 3);

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // Opened first so processInput can ask ImGui whether it owns the keyboard
    // this frame before acting on any key.
    EditorUI::BeginFrame();

    // Walk-mode gravity
    if (!flyMode) {
      verticalVelocity += GRAVITY * deltaTime;
      camera.Position.y += verticalVelocity * deltaTime;
      if (camera.Position.y <= PLAYER_EYE_HEIGHT) {
        camera.Position.y = PLAYER_EYE_HEIGHT;
        verticalVelocity = 0.0f;
        isOnGround = true;
      } else {
        isOnGround = false;
      }
    }

    processInput(window);
    camera.UpdateBob(deltaTime, isMoving, isRunning, isOnGround && !flyMode);

    // Build this frame's panels. Everything below reads the values back out,
    // so edits land on the same frame they're made.
    EditorUI::Draw(editor, scene, camera, deltaTime, uiMode);

    // Swap the environment map if the dropdown moved.
    if (!editor.hdriPaths.empty() && editor.hdriIndex != loadedHdriIndex) {
      envMap = getEnvMap(editor.hdriPaths[editor.hdriIndex]);
      loadedHdriIndex = editor.hdriIndex;
    }

    // Pick each LOD entity's active mesh based on distance to the camera
    // before either render pass draws it this frame.
    scene.UpdateLOD(camera.Position);

    glm::vec3 sunDir = editor.SunDirection(); // direction from world toward sun
    glm::vec3 sunColor = editor.SunRadiance();
    glm::vec3 lightPos = editor.ShadowCameraPos(shadowTarget);

    // Shadow setup
    //  Render depth of scene to texture (from light's perspective)
    // The ortho box has to enclose everything that should cast; its half-width
    // and the light's distance are both editor-driven, so derive the far plane
    // from them instead of hardcoding it.
    float ext = editor.shadowExtent;
    glm::mat4 lightProjection =
        glm::ortho(-ext, ext, -ext, ext, 0.1f, editor.sunDistance + ext * 2.0f);
    // Look toward scene center so the frustum is centered on content.
    glm::mat4 lightView =
        glm::lookAt(lightPos, shadowTarget, glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    //---------------------------PASS 1: SHADOW + RSM DEPTH
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);

    // Activate all 3 RSM color attachments so the clear covers them
    unsigned int rsmDrawBufs[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, rsmDrawBufs);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    rsmDepthShader.use();
    rsmDepthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    rsmDepthShader.setVec3("lightColor", sunColor);

    scene.RenderShadowPass(rsmDepthShader);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

    //---------------------------PASS 2: SCENE GEOMETRY → sceneFBO
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // Activate all 3 attachments so the clear touches all of them
    unsigned int allAttachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, allAttachments);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Skybox only writes to color attachment (no normals or albedo needed for sky)
    unsigned int skyAttachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &skyAttachment);
    glm::mat4 skyView = glm::mat4(glm::mat3(view));
    skyboxShader.use();
    skyboxShader.setMat4("projection", projection);
    skyboxShader.setMat4("view", skyView);
    renderSkybox(skyboxShader, envMap);

    // PBR geometry writes to color, normals, and albedo (MRT)
    unsigned int geoAttachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, geoAttachments);

    pbrShader.use();
    pbrShader.setMat4("projection", projection);
    pbrShader.setMat4("view", view);
    pbrShader.setVec3("camPos", camera.Position);
    pbrShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    pbrShader.setVec3("sunDirection", sunDir);
    pbrShader.setVec3("sunColor", sunColor);
    pbrShader.setFloat("envMapIntensity", editor.envMapIntensity);
    pbrShader.setFloat("minRoughness", editor.minRoughness);
    pbrShader.setFloat("metallicMult", editor.metallicMult);
    for (unsigned int i = 0; i < 2; ++i) {
      pbrShader.setVec3("lightPositions[" + std::to_string(i) + "]", lightPositions[i]);
      pbrShader.setVec3("lightColors["    + std::to_string(i) + "]", lightColors[i]);
    }

    scene.RenderPBRPass(pbrShader, envMap, depthMap);

    glDisable(GL_DEPTH_TEST);
    glm::mat4 invProjection = glm::inverse(projection);
    glm::mat4 invView       = glm::inverse(view);

    //---------------------------PASS 2.5: RSM INDIRECT → rsmIndirectFBO
    glBindFramebuffer(GL_FRAMEBUFFER, rsmIndirectFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    rsmIndirectShader.use();
    rsmIndirectShader.setMat4("invProjection",    invProjection);
    rsmIndirectShader.setMat4("invView",          invView);
    rsmIndirectShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneNormalTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, rsmPosTex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, rsmNormalTex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, rsmFluxTex);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    //---------------------------PASS 2.6: SSGI → ssgiFBO
    glBindFramebuffer(GL_FRAMEBUFFER, ssgiFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    ssgiShader.use();
    ssgiShader.setMat4("projection",    projection);
    ssgiShader.setMat4("invProjection", invProjection);
    ssgiShader.setFloat("time", (float)glfwGetTime());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneNormalTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, sceneAlbedoTex);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    //---------------------------PASS 3: SSR + SSGI + RSM COMPOSITE → default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    ssrShader.use();
    ssrShader.setMat4("projection",    projection);
    ssrShader.setMat4("invProjection", invProjection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneNormalTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, ssgiColorTex);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, rsmIndirectTex);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);

    // Editor panels go last so they sit on top of the composited frame.
    EditorUI::EndFrame();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  EditorUI::Shutdown();
  glfwTerminate();
  return 0;
}

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  // Tab hands the cursor to the editor panels and back. ImGui gets first
  // refusal so Tab still walks between fields while one is being typed into.
  bool tabPressed = !EditorUI::WantsKeyboard() &&
                    glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
  if (tabPressed && !tabPreviouslyPressed) {
    uiMode = !uiMode;
    glfwSetInputMode(window, GLFW_CURSOR,
                     uiMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    // The cursor moved freely while the UI had it; don't feed that travel to
    // the camera as one huge delta when it gets recaptured.
    firstMouse = true;
  }
  tabPreviouslyPressed = tabPressed;

  // While the UI owns the cursor the camera takes no input at all.
  if (uiMode) {
    isMoving = false;
    isRunning = false;
    return;
  }

  // Toggle fly / walk mode
  bool qPressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
  if (qPressed && !qPreviouslyPressed) {
    flyMode = !flyMode;
    if (flyMode)
      verticalVelocity = 0.0f;
  }
  qPreviouslyPressed = qPressed;

  isRunning = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
              glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

  bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  isMoving = w || s || a || d;

  float speedMult = isRunning ? RUN_SPEED_MULT : 1.0f;

  if (flyMode) {
    float velocity = camera.MovementSpeed * speedMult * deltaTime;
    if (w) camera.Position += camera.Front  * velocity;
    if (s) camera.Position -= camera.Front  * velocity;
    if (a) camera.Position -= camera.Right  * velocity;
    if (d) camera.Position += camera.Right  * velocity;
  } else {
    float velocity = camera.MovementSpeed * speedMult * deltaTime;
    glm::vec3 flatFront = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
    glm::vec3 flatRight = glm::normalize(glm::vec3(camera.Right.x, 0.0f, camera.Right.z));

    if (w) camera.Position += flatFront * velocity;
    if (s) camera.Position -= flatFront * velocity;
    if (a) camera.Position -= flatRight * velocity;
    if (d) camera.Position += flatRight * velocity;

    // Jump
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isOnGround) {
      verticalVelocity = JUMP_FORCE;
      isOnGround = false;
    }
  }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
  // Cursor belongs to the panels in editor mode — don't turn the camera with it.
  if (uiMode)
    return;

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos;

  lastX = xpos;
  lastY = ypos;

  camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  // Let the wheel scroll a panel instead of zooming when it's over one.
  if (EditorUI::WantsMouse())
    return;

  camera.ProcessMouseScroll(yoffset);
}

