#include "camera.h"
#include "model.h"
#include "scene_manager.h"
#include "shader.h"
#include "test_callback.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
// #include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);

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

// Helper function to render scene
SceneUtils sceneRender = SceneUtils();

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

  glEnable(GL_DEPTH_TEST);
  // glEnable(GL_CULL_FACE);

  // Build and compile shaders
  Shader pbrShader("shaders/pbr.vs", "shaders/pbr.fs");
  Shader simpleDepthShader("shaders/shadow_depth.vs", "shaders/shadow_depth.fs");
  Shader rsmDepthShader("shaders/rsm_depth.vs", "shaders/rsm_depth.fs");
  Shader rsmIndirectShader("shaders/ssr.vs", "shaders/rsm_indirect.fs");
  Shader ssrShader("shaders/ssr.vs", "shaders/ssr.fs");
  Shader ssgiShader("shaders/ssr.vs", "shaders/ssgi.fs");

  // Load multiple models (can be same file or different)
  Model model1("ground", "models/plane/simple_plane.obj");
  Model model2("cup", "models/cup/cup.obj");
  Model model3("table", "models/table/table.obj");
  Model model4("rock", "models/coast_rock/coast_rock.obj");

  /////////env map///////
  Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");
  initSkybox();
  initSceneFBO();
  initQuad();
  initRSMIndirectFBO();
  initSSGIFBO();

  // Load environment map 
  unsigned int envMap = loadEquirectangularMap("models/env_map.hdr");

  // vector<Model*> models {&model1, &model2, &model3};

  // Load textures
  unsigned int albedo = loadTexture("models/plane/albedo.png");
  unsigned int normal = loadTexture("models/plane/normal.png");
  unsigned int metallic = loadTexture("models/plane/metallic.png");
  unsigned int roughness = loadTexture("models/plane/roughness.png");
  unsigned int ao = loadTexture("models/plane/ao.png");

  unsigned int cup_albedo = loadTexture("models/cup/albedo.png");
  unsigned int cup_normal = loadTexture("models/cup/normal.png");
  unsigned int cup_metallic = loadTexture("models/cup/metallic.png");
  unsigned int cup_roughness = loadTexture("models/cup/roughness.png");
  unsigned int cup_ao = loadTexture("models/cup/ao.png");

  unsigned int table_albedo = loadTexture("models/table/albedo.png");
  unsigned int table_normal = loadTexture("models/table/normal.png");
  unsigned int table_metallic = loadTexture("models/table/metallic.png");
  unsigned int table_roughness = loadTexture("models/table/roughness.png");
  unsigned int table_ao = loadTexture("models/table/ao.png");

  unsigned int rock_albedo = loadTexture("models/coast_rock/albedo.png");
  unsigned int rock_normal = loadTexture("models/coast_rock/normal.png");
  unsigned int rock_metallic = loadTexture("models/coast_rock/metallic.png");
  unsigned int rock_roughness = loadTexture("models/coast_rock/roughness.png");
  unsigned int rock_ao = loadTexture("models/coast_rock/ao.png");

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

  // Directional sun light — lightPos only used for shadow lookAt, not for shading
  const float sunLightValue = 5.0f;
  glm::vec3 lightPos(-10.0f, 30.0f, -10.0f);
  glm::vec3 sunDir = glm::normalize(lightPos); // direction from world toward sun
  glm::vec3 sunColor = glm::vec3(sunLightValue);

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
  pbrShader.setFloat("envMapIntensity", 1.0f);
  pbrShader.setFloat("minRoughness",    0.35f); // prevents mirror-smooth surfaces
  pbrShader.setFloat("metallicMult",    0.5f);  // table textures run ~0.81 mean; pull back

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

    // Shadow setup
    //  Render depth of scene to texture (from light's perspective)
    // Ortho bounds must enclose the entire scene; rock is at x=12 so use ±35.
    // Far plane extended to 100 so nothing gets clipped along the light ray.
    glm::mat4 lightProjection =
        glm::ortho(-35.0f, 35.0f, -35.0f, 35.0f, 0.1f, 100.0f);
    // Look toward scene center (~6,0,0) so the frustum is centered on content.
    glm::mat4 lightView =
        glm::lookAt(lightPos, glm::vec3(6.0f, 0.0f, 0.0f), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    //---------------------------3D OBJECTS
    //XFORMS------------------------------------------------ ground
    const glm::vec3 model1_position{0.0f, 0.0f, 0.0f};
    const glm::vec3 model1_scale{0.8f};
    // cup
    const glm::vec3 model2_position{1.0f, 2.05f, 0.0f};
    const glm::vec3 model2_scale{0.5f};
    // table
    const glm::vec3 model3_position{1.0f, 1.0f, 0.0f};
    const glm::vec3 model3_scale{0.02f};
    // rock
    const glm::vec3 model4_position{12.0f, 0.0f, 0.0f};
    const glm::vec3 model4_scale{1.0f};

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

    // Bind albedo per model; renderModel sets the "model" matrix uniform internally
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo);
    sceneRender.renderModel(rsmDepthShader, &model1, model1_position, model1_scale);
    glBindTexture(GL_TEXTURE_2D, cup_albedo);
    sceneRender.renderModel(rsmDepthShader, &model2, model2_position, model2_scale);
    glBindTexture(GL_TEXTURE_2D, table_albedo);
    sceneRender.renderModel(rsmDepthShader, &model3, model3_position, model3_scale);
    glBindTexture(GL_TEXTURE_2D, rock_albedo);
    sceneRender.renderModel(rsmDepthShader, &model4, model4_position, model4_scale);

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
    for (unsigned int i = 0; i < 2; ++i) {
      pbrShader.setVec3("lightPositions[" + std::to_string(i) + "]", lightPositions[i]);
      pbrShader.setVec3("lightColors["    + std::to_string(i) + "]", lightColors[i]);
    }

    sceneRender.processShaderPipeline(envMap, albedo, normal, metallic, roughness, ao,
                                      depthMap, pbrShader, &model1, model1_position, model1_scale);
    sceneRender.processShaderPipeline(envMap, cup_albedo, cup_normal, cup_metallic, cup_roughness, cup_ao,
                                      depthMap, pbrShader, &model2, model2_position, model2_scale);
    sceneRender.processShaderPipeline(envMap, table_albedo, table_normal, table_metallic, table_roughness, table_ao,
                                      depthMap, pbrShader, &model3, model3_position, model3_scale);
    sceneRender.processShaderPipeline(envMap, rock_albedo, rock_normal, rock_metallic, rock_roughness, rock_ao,
                                      depthMap, pbrShader, &model4, model4_position, model4_scale);

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

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

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
  camera.ProcessMouseScroll(yoffset);
}

unsigned int loadTexture(const char *path) {
  unsigned int textureID;
  glGenTextures(1, &textureID);

  int width, height, nrComponents;
  unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
  if (data) {
    GLenum format;
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    float maxAniso;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);

    stbi_image_free(data);
  } else {
    std::cout << "Texture failed to load at path: " << path << std::endl;
    stbi_image_free(data);
  }

  return textureID;
}
