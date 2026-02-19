#include "model.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "camera.h"
#include "test_callback.h"
#include "scene_manager.h"
#include <iostream>
//#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path);

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(glm::vec3(0.0f, 2.0f, 10.0f));
float  lastX = SCR_WIDTH / 2.0f;
float  lastY = SCR_HEIGHT / 2.0f;
bool   firstMouse = true;

float  deltaTime = 0.0f;
float  lastFrame = 0.0f;

// Shadow map dimensions
const unsigned int shadow_dim {1024}; 
const unsigned int SHADOW_WIDTH = shadow_dim, SHADOW_HEIGHT = shadow_dim;
unsigned int       depthMapFBO;
unsigned int       depthMap;

// Helper function to render scene 
SceneUtils sceneRender = SceneUtils();

int main() {
    TestCallback test1 = TestCallback();
    test1.PrintTest();
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RETINAL ENGINE", NULL, NULL);
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
    glEnable(GL_CULL_FACE);

    // Build and compile shaders
    Shader pbrShader("shaders/pbr.vs", "shaders/pbr.fs");
    Shader simpleDepthShader("shaders/shadow_depth.vs", "shaders/shadow_depth.fs");

    // Load multiple models (can be same file or different)
    Model model1("ground","models/plane/simple_plane.obj");  
    Model model2("cup", "models/cup/cup.obj");
    Model model3("table", "models/table/table.obj"); 

    //vector<Model*> models {&model1, &model2, &model3};

    
    // Load textures
    unsigned int albedo          = loadTexture("models/plane/albedo.png");
    unsigned int normal          = loadTexture("models/plane/normal.png");
    unsigned int metallic        = loadTexture("models/plane/metallic.png");
    unsigned int roughness       = loadTexture("models/plane/roughness.png");
    unsigned int ao              = loadTexture("models/plane/ao.png");

    unsigned int cup_albedo      = loadTexture("models/cup/albedo.png");
    unsigned int cup_normal      = loadTexture("models/cup/normal.png");
    unsigned int cup_metallic    = loadTexture("models/cup/metallic.png");
    unsigned int cup_roughness   = loadTexture("models/cup/roughness.png");
    unsigned int cup_ao          = loadTexture("models/cup/ao.png");

    unsigned int table_albedo    = loadTexture("models/table/albedo.png");
    unsigned int table_normal    = loadTexture("models/table/normal.png");
    unsigned int table_metallic  = loadTexture("models/table/metallic.png");
    unsigned int table_roughness = loadTexture("models/table/roughness.png");
    unsigned int table_ao        = loadTexture("models/table/ao.png");


    // Configure depth map FBO
    glGenFramebuffers (1, &depthMapFBO);
    glGenTextures     (1, &depthMap);
    glBindTexture     (GL_TEXTURE_2D, depthMap);
    glTexImage2D      (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri   (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri   (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri   (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri   (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Light setup (use first light as "sun" for shadows)
    glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);
    glm::vec3 lightColors[] = {
        glm::vec3(300.0f, 300.0f, 300.0f),
        glm::vec3(100.0f, 100.0f, 100.0f),
        glm::vec3(100.0f, 100.0f, 100.0f)
    };
    glm::vec3 lightPositions[] = {
        lightPos,
        glm::vec3(10.0f, -10.0f, 10.0f),
        glm::vec3(-10.0f, 10.0f, 10.0f)
    };

    // Configure PBR shader
    pbrShader.use();
    pbrShader.setInt("albedoMap", 0);
    pbrShader.setInt("normalMap", 1);
    pbrShader.setInt("metallicMap", 2);
    pbrShader.setInt("roughnessMap", 3);
    pbrShader.setInt("aoMap", 4);
    pbrShader.setInt("shadowMap", 5);  // Shadow map in here :)

    while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput(window);

    //Shadow setup
    // Render depth of scene to texture (from light's perspective)
    glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 7.5f);
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    
    simpleDepthShader.use();
    simpleDepthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);  // Prevent peter-panning
    // renderScene(simpleDepthShader, model1, model2, model3); //hard coded 3 models, to fix this <-
    //sceneRender.renderScene(simpleDepthShader, models); //hard coded 3 models, to fix this <-
    
    //---------------------------3D OBJECTS XFORMS------------------------------------------------
    //ground 
    const glm::vec3 model1_position {0.0f, 0.0f, 0.0f};
    const glm::vec3 model1_scale {0.2f};
    //cup
    const glm::vec3 model2_position {1.0f, 2.05f, 0.0f};
    const glm::vec3 model2_scale {0.5f};
    //table
    const glm::vec3 model3_position {1.0f, 1.0f, 0.0f};
    const glm::vec3 model3_scale {0.02f};


    //---------------------------RENDER SHADOW DEPTH PIPELINE--------------------------------------
    
    sceneRender.renderModel(simpleDepthShader, &model1, model1_position, model1_scale);
    sceneRender.renderModel(simpleDepthShader, &model2, model2_position, model2_scale);
    sceneRender.renderModel(simpleDepthShader, &model3, model3_position, model3_scale);


    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Render scene as normal with shadow mapping
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    pbrShader.use();
    // pbrShader_cup.use();
    // pbrShader_table.use();

    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();
    pbrShader.setMat4("projection", projection);
    pbrShader.setMat4("view", view);
    pbrShader.setVec3("camPos", camera.Position);
    pbrShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    
    // Set lights
    for (unsigned int i = 0; i < 3; ++i) {
        pbrShader.setVec3("lightPositions[" + std::to_string(i) + "]", lightPositions[i]);
        pbrShader.setVec3("lightColors[" + std::to_string(i) + "]", lightColors[i]);
    }

    //---------------------------RENDER SHADER GEOM PIPELINE--------------------------------------
    // Bind textures

    sceneRender.processShaderPipeline(albedo, normal, metallic, roughness, ao, depthMap, 
        pbrShader, &model1, model1_position, model1_scale);

    sceneRender.processShaderPipeline(cup_albedo, cup_normal, cup_metallic, cup_roughness, cup_ao, depthMap,
        pbrShader, &model2, model2_position, model2_scale);

    sceneRender.processShaderPipeline(table_albedo, table_normal, table_metallic, table_roughness, table_ao, depthMap,
        pbrShader, &model3, model3_position, model3_scale);


    glfwSwapBuffers(window);
    glfwPollEvents();
}

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
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

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
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
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
