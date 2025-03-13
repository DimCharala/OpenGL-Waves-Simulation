// Include C++ headers
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib> 

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <glfw3.h>

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Shader loading utilities and other
#include <common/shader.h>
#include <common/util.h>
#include <common/camera.h>
#include <common/model.h>
#include <common/texture.h>
#include <common/light.h>
#include <common/FountainEmitter.h>
#include "stb_image_aug.h"
#include <algorithm>

//Enable for particles
//#define PARTICLES 


using namespace std;
using namespace glm;

// Function prototypes
void initialize();
void createContext();
void mainLoop();
void free();
void createWaves(int waveCount);
void uploadWavesToShader(GLuint shaderProgram);
vec2 rotateVector(const vec2& v, float angle);

#define W_WIDTH 1024
#define W_HEIGHT 768
#define TITLE "Waves"


// Global variables
GLFWwindow* window;
Camera* camera;
Light* light;
GLuint shaderProgram, depthProgram, miniMapProgram, particleShaderProgram;
GLuint viewMatrixLocation;
GLuint projectionMatrixLocation;
GLuint modelMatrixLocation;

GLuint shadowViewProjectionLocation;
GLuint shadowModelLocation;
GLuint depthFBO, depthTexture;

GLuint quadTextureSamplerLocation;
Drawable* quad;

GLuint textureSampler;
GLuint texture;
GLuint movingTexture;
GLuint movingTextureSampler;
GLuint displacementTexture;
GLuint displacementTextureSampler;
GLuint foamTexture;
GLuint foamTextureSampler;
GLuint foamFBO, foamRBO, foamTextureMap;
GLuint projectionAndViewMatrix;


GLuint shadowMapSampler;
GLuint roughnessTexture;
GLuint occTexture;
GLuint normalTexture;
GLuint roughnessTextureSampler;
GLuint occTextureSampler;
GLuint normalTextureSampler;

GLuint timeUniform;

GLuint skyboxVAO, skyboxVBO, skyboxEBO;
GLuint skyboxProgram;
GLuint cubemapTexture;

GLuint LaLocation, LdLocation, LsLocation;
GLuint lightPositionLocation;
GLuint lightPowerLocation;

GLuint depthMapSampler;
GLuint lightVPLocation;

GLuint wavesVAO;
GLuint wavesVBO, wavesIBO, wavesUVBO;
GLuint waveMeshLength;  // Renamed from 'length' to 'waveMeshLength'
GLuint waveTimeLocation;
GLuint wavesLocation;

//#define PARTICLES
#ifdef PARTICLES
int N = 6;
#else
int N = 9;
#endif 

//#define LINE
#define FILL

int sideSlices = pow(2, N);

#define Wave1
#ifdef Wave1
float waveAmplitude = 1.5f / 6;
float waveSteepness = 0.1f;
float waveLength = 8.0f;
float waveSpeed = 1.75f;
#endif

#ifdef Wave2
float waveAmplitude = 1.0f / 6;
float waveSteepness = 0.05f;
float waveLength = 10.0f;
float waveSpeed = 1.5f;
#endif

#ifdef Wave3
float waveAmplitude = 2.0f / 6;
float waveSteepness = 0.2f;
float waveLength = 9.0f;
float waveSpeed = 2.0f;
#endif



vector<FountainEmitter> emitters;


vector<vec3> vertices;
vector<uvec3> indices;
vector<vec2> uvs;


#define SHADOW_WIDTH 2048
#define SHADOW_HEIGHT 2048

struct Wave {
    vec2 direction;
    float steepness;
    float wavelength;
    float speed;
    float amplitude;
};

float directionX = 1.0f;
float directionZ = 1.0f;
int waveCount = 300;
vector<Wave> waves;


vec2 primaryDirection = vec2(0.75f, 1.0f);

#include <cstdlib> // For rand()

vec2 rotateVector(const vec2& v, float angle) {
    float cosTheta = cos(angle);
    float sinTheta = sin(angle);
    return vec2(
        v.x * cosTheta - v.y * sinTheta,
        v.x * sinTheta + v.y * cosTheta
    );
}

void createWaves(int waveCount) {
    waves.clear();
    float baseSteepness = waveSteepness;
    float baseWavelength = waveLength;
    float baseSpeed = waveSpeed;
    float baseAmplitude = waveAmplitude;

    // Parameters for frequency and amplitude decay (similar to FBM)
    float amplitudeDecay = 0.85f;  // Each successive wave will have half the amplitude of the previous
    float frequencyMultiplier = 1.15f;  // Frequency doubles for each successive wave

    for (int i = 0; i < waveCount; i++) {
        Wave wave;

        // Generate a random direction (angle in radians between 0 and 2π)
        float randomAngle = ((rand() % 100) / 100.0f) * (2.0f * 3.14159265f);
        wave.direction = vec2(cos(randomAngle), sin(randomAngle));

        // Apply amplitude decay and frequency increase as per FBM
        wave.amplitude = baseAmplitude * pow(amplitudeDecay, i);  // Amplitude decreases exponentially
        wave.wavelength = baseWavelength / pow(frequencyMultiplier, i);  // Frequency (or 1/wavelength) increases exponentially
        wave.speed = baseSpeed * sqrt((9.81f * wave.wavelength) / (2.0f * 3.14159265f));

        // Slight variations in steepness for realism
        wave.steepness = baseSteepness * (0.9f + (rand() % 20 / 100.0f));

        waves.push_back(wave);
    }
}



// Function to upload waves to the shader
void uploadWavesToShader(GLuint shaderProgram) {
    glUseProgram(shaderProgram);
    GLint waveCountLocation = glGetUniformLocation(shaderProgram, "n");
    glUniform1i(waveCountLocation, waveCount);

    for (int i = 0; i < waveCount; i++) {
        string prefix = "waves[" + to_string(i) + "].";
        glUniform2fv(glGetUniformLocation(shaderProgram, (prefix + "direction").c_str()), 1, &waves[i].direction[0]);
        glUniform1f(glGetUniformLocation(shaderProgram, (prefix + "steepness").c_str()), waves[i].steepness);
        glUniform1f(glGetUniformLocation(shaderProgram, (prefix + "wavelength").c_str()), waves[i].wavelength);
        glUniform1f(glGetUniformLocation(shaderProgram, (prefix + "speed").c_str()), waves[i].speed);
        glUniform1f(glGetUniformLocation(shaderProgram, (prefix + "amplitude").c_str()), waves[i].amplitude);
    }
}

void uploadLight(const Light& light) {
    glUniform4f(LaLocation, light.La.r, light.La.g, light.La.b, light.La.a);
    glUniform4f(LdLocation, light.Ld.r, light.Ld.g, light.Ld.b, light.Ld.a);
    glUniform4f(LsLocation, light.Ls.r, light.Ls.g, light.Ls.b, light.Ls.a);
    glUniform3f(lightPositionLocation, light.lightPosition_worldspace.x,
        light.lightPosition_worldspace.y, light.lightPosition_worldspace.z);
    glUniform1f(lightPowerLocation, light.power);
}



void createContext() {
    // Create and compile our GLSL program from the shaders
    shaderProgram = loadShaders("texture.vertexshader", "texture.fragmentshader");
    depthProgram = loadShaders("Depth.vertexshader", "Depth.fragmentshader");
    miniMapProgram = loadShaders("SimpleTexture.vertexshader", "SimpleTexture.fragmentshader");
    particleShaderProgram = loadShaders("particleSystem.vertexshader", "particleSystem.fragmentshader");
    skyboxProgram = loadShaders("skybox.vertexshader", "skybox.fragmentshader");
    // Draw wireframe triangles or fill: GL_LINE, or GL_FILL
#ifdef FILL
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

#ifdef LINE
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

#endif // LINE


    // Get a pointer location to model matrix in the vertex shader
    projectionMatrixLocation = glGetUniformLocation(shaderProgram, "P");
    viewMatrixLocation = glGetUniformLocation(shaderProgram, "V");
    modelMatrixLocation = glGetUniformLocation(shaderProgram, "M");

    depthMapSampler = glGetUniformLocation(shaderProgram, "shadowMapSampler");
    lightVPLocation = glGetUniformLocation(shaderProgram, "lightVP");

    LaLocation = glGetUniformLocation(shaderProgram, "light.La");
    LdLocation = glGetUniformLocation(shaderProgram, "light.Ld");
    LsLocation = glGetUniformLocation(shaderProgram, "light.Ls");
    lightPositionLocation = glGetUniformLocation(shaderProgram, "light.lightPosition_worldspace");
    lightPowerLocation = glGetUniformLocation(shaderProgram, "light.power");

    //https://3dtextures.me/2018/11/29/water-002/
    movingTexture = loadBMP("Water_002_COLOR.bmp");
    displacementTexture = loadBMP("oceandisplacement.bmp");
    foamTexture = loadBMP("foamTest.bmp"); // Load foam texture
    movingTextureSampler = glGetUniformLocation(shaderProgram, "movingTextureSampler");
    displacementTextureSampler = glGetUniformLocation(shaderProgram, "displacementTextureSampler");
    foamTextureSampler = glGetUniformLocation(shaderProgram, "foamTextureSampler"); // Foam texture sampler
    shadowMapSampler = glGetUniformLocation(shaderProgram, "shadowMapSampler");

    roughnessTexture = loadBMP("Water_002_ROUGH.bmp");
    occTexture = loadBMP("Water_002_OCC.bmp");
    normalTexture = loadBMP("Water_002_NORM.bmp");
    roughnessTextureSampler = glGetUniformLocation(shaderProgram, "roughnessTextureSampler");
    occTextureSampler = glGetUniformLocation(shaderProgram, "occTextureSampler");
    normalTextureSampler = glGetUniformLocation(shaderProgram, "normalTextureSampler");


    waveTimeLocation = glGetUniformLocation(shaderProgram, "waveTime");
    wavesLocation = glGetUniformLocation(shaderProgram, "waves");

    // Shadow shader
    shadowViewProjectionLocation = glGetUniformLocation(depthProgram, "VP");
    shadowModelLocation = glGetUniformLocation(depthProgram, "M");

    quadTextureSamplerLocation = glGetUniformLocation(miniMapProgram, "textureSampler");


    projectionAndViewMatrix = glGetUniformLocation(particleShaderProgram, "PV");


    vector<vec3> quadVertices = {
        vec3(0.5, 0.5, -1.0),
        vec3(1.0, 0.5, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(0.5, 1.0, -1.0),
        vec3(0.5, 0.5, -1.0)
    };

    vector<vec2> quadUVs = {
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0),
        vec2(0.0, 0.0)
    };

    quad = new Drawable(quadVertices, quadUVs);

    glGenVertexArrays(1, &wavesVAO);
    glBindVertexArray(wavesVAO);

    // Grid initialization
    for (int j = 0; j <= sideSlices; ++j) {
        for (int i = 0; i <= sideSlices; ++i) {
            float x = ((float)i / (float)sideSlices) * 2.5 * N;
            float y = 0;
            float z = ((float)j / (float)sideSlices) * 2.5 * N;
            vertices.push_back(glm::vec3(x, y, z));

            float u = -glm::clamp((float)i / (float)sideSlices, 0.0f, 1.0f);
            float v = -glm::clamp((float)j / (float)sideSlices, 0.0f, 1.0f);
            uvs.push_back(glm::vec2(u, v));
        }
    }

    // Creation of triangles using indices
    for (int j = 0; j < sideSlices; ++j) {
        for (int i = 0; i < sideSlices; ++i) {
            int row1 = j * (sideSlices + 1);
            int row2 = (j + 1) * (sideSlices + 1);

            // First triangle
            indices.push_back(glm::uvec3(row1 + i, row1 + i + 1, row2 + i + 1));

            // Second triangle
            indices.push_back(glm::uvec3(row1 + i, row2 + i + 1, row2 + i));
        }
    }

    glGenBuffers(1, &wavesVBO);
    glBindBuffer(GL_ARRAY_BUFFER, wavesVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Create buffer for UV coordinates
    glGenBuffers(1, &wavesUVBO);
    glBindBuffer(GL_ARRAY_BUFFER, wavesUVBO);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), uvs.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenBuffers(1, &wavesIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wavesIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(glm::uvec3), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    waveMeshLength = (GLuint)indices.size() * 3; // Renamed variable to avoid ambiguity

    glGenFramebuffers(1, &depthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Attach the depth texture as the framebuffer's depth buffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    // No color buffer is drawn to
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    float skyboxVertices[] =
    {
        //   Coordinates
        -1.0f, -1.0f,  1.0f,//        7--------6
         1.0f, -1.0f,  1.0f,//       /|       /|
         1.0f, -1.0f, -1.0f,//      4--------5 |
        -1.0f, -1.0f, -1.0f,//      | |      | |
        -1.0f,  1.0f,  1.0f,//      | 3------|-2
         1.0f,  1.0f,  1.0f,//      |/       |/
         1.0f,  1.0f, -1.0f,//      0--------1
        -1.0f,  1.0f, -1.0f
    };

    unsigned int skyboxIndices[] =
    {
        // Right
        1, 2, 6,
        6, 5, 1,
        // Left
        0, 4, 7,
        7, 3, 0,
        // Top
        4, 5, 6,
        6, 7, 4,
        // Bottom
        0, 3, 2,
        2, 1, 0,
        // Back
        0, 1, 5,
        5, 4, 0,
        // Front
        3, 7, 6,
        6, 2, 3
    };

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glGenBuffers(1, &skyboxEBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), &skyboxIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


    vector<std::string> faces
    {
        "right.jpg",
        "left.jpg",
        "top.jpg",
        "bottom.jpg",
        "front.jpg",
        "back.jpg"
    };

    
    glGenTextures(1, &cubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // These are very important to prevent seams
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    for (unsigned int i = 0; i < 6; i++)
    {
        int width, height, nrChannels;
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            std::cout << "Succeeded to load texture: " << faces[i] << std::endl;
            glTexImage2D
            (
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                GL_RGB,
                width,
                height,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                data
            );
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Failed to load texture: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }



    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glfwTerminate();
        throw runtime_error("Frame buffer not initialized correctly");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void free() {
    // Delete Vertex Arrays
    glDeleteVertexArrays(1, &wavesVAO);
    glDeleteVertexArrays(1, &skyboxVAO);

    // Delete Buffers
    glDeleteBuffers(1, &wavesVBO);
    glDeleteBuffers(1, &wavesUVBO);
    glDeleteBuffers(1, &wavesIBO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteBuffers(1, &skyboxEBO);

    // Delete Textures
    glDeleteTextures(1, &texture);
    glDeleteTextures(1, &movingTexture);
    glDeleteTextures(1, &displacementTexture);
    glDeleteTextures(1, &foamTexture); // Foam texture
    glDeleteTextures(1, &depthTexture); // Shadow map texture
    glDeleteTextures(1, &cubemapTexture); // Skybox texture
    glDeleteTextures(1, &roughnessTexture);
    glDeleteTextures(1, &occTexture);
    glDeleteTextures(1, &normalTexture);

    // Delete Framebuffers
    glDeleteFramebuffers(1, &depthFBO);
    glDeleteRenderbuffers(1, &foamRBO); // Foam Render Buffer (if applicable)

    // Delete Shader Programs
    glDeleteProgram(shaderProgram);
    glDeleteProgram(depthProgram);
    glDeleteProgram(miniMapProgram);
    glDeleteProgram(particleShaderProgram);
    glDeleteProgram(skyboxProgram);

    // Free dynamically allocated objects
    if (camera) {
        delete camera;
        camera = nullptr;
    }
    if (light) {
        delete light;
        light = nullptr;
    }
    if (quad) {
        delete quad;
        quad = nullptr;
    }

    // Terminate GLFW
    glfwTerminate();
}


void depth_pass(mat4 viewMatrix, mat4 projectionMatrix) {

    // Task 3.3
    //*/


    // Setting viewport to shadow map size
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);

    // Binding the depth framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

    // Cleaning the framebuffer depth information (stored from the last render)
    glClear(GL_DEPTH_BUFFER_BIT);

    // Selecting the new shader program that will output the depth component
    glUseProgram(depthProgram);

    // sending the view and projection matrix to the shader
    mat4 view_projection = projectionMatrix * viewMatrix;
    glUniformMatrix4fv(shadowViewProjectionLocation, 1, GL_FALSE, &view_projection[0][0]);


    // Set the model matrix
    mat4 model = mat4(1.0f);
    glUniformMatrix4fv(shadowModelLocation, 1, GL_FALSE, &model[0][0]);

    // Render the waves for shadow mapping
    glBindVertexArray(wavesVAO);
    glDrawElements(GL_TRIANGLES, indices.size() * 3, GL_UNSIGNED_INT, nullptr);

    // Unbind the framebuffer to stop rendering to the depth texture
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Reset to the main shader program if needed
    glUseProgram(shaderProgram);
}

void waveUpdate() {
    glUniform1f(waveTimeLocation, (float)glfwGetTime() / 20.0);
    mat4 MVP, modelMatrix, viewMatrix, projectionMatrix;

    projectionMatrix = perspective(radians(45.0f), 4.0f / 3.0f, 0.1f, 100.0f);
    viewMatrix = lookAt(
        vec3(5 * sin((float)glfwGetTime()), 0, 5 * cos((float)glfwGetTime())),
        vec3(0, 0, 0),
        vec3(0, 1, 0)
    );
    modelMatrix = mat4(1.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W_WIDTH, W_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);

    uploadWavesToShader(shaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, movingTexture);
    glUniform1i(movingTextureSampler, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, displacementTexture);
    glUniform1i(displacementTextureSampler, 1);

    glActiveTexture(GL_TEXTURE2);  // Bind foam texture to unit 2
    glBindTexture(GL_TEXTURE_2D, foamTexture);
    glUniform1i(foamTextureSampler, 2);

    glActiveTexture(GL_TEXTURE3);  // Bind to texture unit 3
    glBindTexture(GL_TEXTURE_2D, depthTexture);  // Bind the shadow map texture
    glUniform1i(shadowMapSampler, 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glUniform1i(glGetUniformLocation(shaderProgram, "skybox"), 4);

    
    camera->update();
    projectionMatrix = camera->projectionMatrix;
    viewMatrix = camera->viewMatrix;
    modelMatrix = glm::mat4(1.0);

    glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &projectionMatrix[0][0]);
    glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, &modelMatrix[0][0]);

    glBindVertexArray(wavesVAO);
    glDrawElements(GL_TRIANGLES, indices.size() * 3, GL_UNSIGNED_INT, nullptr);
}

void lighting_pass(mat4 viewMatrix, mat4 projectionMatrix) {


    // Step 1: Binding a frame buffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W_WIDTH, W_HEIGHT);

    // Step 2: Clearing color and depth info
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Step 3: Selecting shader program
    glUseProgram(shaderProgram);


    // Making view and projection matrices uniform to the shader program
    glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &projectionMatrix[0][0]);

    // uploading the light parameters to the shader program
    uploadLight(*light);


    // Task 4.1 Display shadows on the 
    //*/
    // Sending the shadow texture to the shaderProgram
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glUniform1i(depthMapSampler, 0);

    // Sending the light View-Projection matrix to the shader program
    mat4 lightVP = light->lightVP();
    glUniformMatrix4fv(lightVPLocation, 1, GL_FALSE, &lightVP[0][0]);

    //*/




    // ----------------------------------------------------------------- //
    // --------------------- Drawing scene objects --------------------- //	
    // ----------------------------------------------------------------- //


    // Task 1.3 Draw a plane under suzanne
    // Create an identity model matrix
    // Reset the model matrix to identity
    mat4 planeModelMatrix = mat4(1.0f);

    // Apply scaling by a factor of 10
    mat4 scaleMatrix = scale(mat4(1.0f), vec3(1.0f, 1.0f, 1.0f));

    // Apply translation to move by -5 units in the Y-axis
    mat4 translationMatrix = translate(mat4(1.0f), vec3(0.0f, -5.0f, 0.0f));

    // Combine scaling and translation into the model matrix
    //planeModelMatrix = translationMatrix * scaleMatrix;


    // upload the model matrix
    glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, &planeModelMatrix[0][0]);

    glBindVertexArray(wavesVAO);
    glDrawElements(GL_TRIANGLES, indices.size() * 3, GL_UNSIGNED_INT, nullptr);
}

void renderDepthMap() {
    glUseProgram(miniMapProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glUniform1i(quadTextureSamplerLocation, 0);

    quad->bind();
    quad->draw();
    glUseProgram(shaderProgram);
}


void initializeEmitters(const vector<vec3>& topVertices) {
    emitters.clear();  // Ensure the emitters vector is empty
    for (int i = 0; i < topVertices.size(); ++i) {
        FountainEmitter emitter = FountainEmitter(quad, 25);  // Create each emitter
        emitter.emitter_pos = topVertices[i];  // Assign position directly from top vertices
        emitter.use_rotations = true;
        emitter.use_sorting = false;
        emitter.height_threshold = 0.05f;  // Adjust as needed
        emitters.push_back(emitter);  // Store the emitter in the vector
    }
}

void updateEmitters(const vector<vec3>& vertices) {
    // Assuming emitters.size() == vertices.size()
    for (size_t i = 0; i < emitters.size(); ++i) {
        // Update emitter position with wave height from the vertices
        vec3 newPos = vertices[i];
        cout << newPos.x << " " << newPos.z << endl;

        // Assign position directly to the emitter
        emitters[i].emitter_pos = vec3(newPos.x*2,newPos.y,newPos.z*2);

        // Optional: You can also adjust the height threshold to control when bubbles form
        emitters[i].height_threshold = 0.05f;  // Adjust threshold as needed

        emitters[i].use_rotations = true;
        emitters[i].use_sorting = false;
    }
}

float calculateHeight(const vec3& vertex, const vector<Wave>& waves, float time) {
    float height = 0.0f;

    for (const auto& wave : waves) {
        // Gerstner wave height calculation (as an example)
        float wavePhase = dot(wave.direction, vec2(vertex.x, vertex.z)) - wave.speed * time;
        height += wave.amplitude * sin(wavePhase);
    }

    return height;
}

vector<vec3> findTopVertices(const vector<vec3>& vertices, const vector<Wave>& waves, float time, int topCount = 10) {
    vector<pair<float, vec3>> vertexHeights;

    // Calculate the height for each vertex
    for (const auto& vertex : vertices) {
        float height = calculateHeight(vertex, waves, time);
        vertexHeights.push_back(make_pair(height, vertex));
    }
    
    // Sort vertices by their calculated height
    std::sort(vertexHeights.begin(), vertexHeights.end(), [](const pair<float, vec3>& a, const pair<float, vec3>& b) {
        return a.first > b.first;  // Sort by height in descending order
        });

    // Extract the top 50 vertices
    vector<vec3> topVertices;
    for (int i = 0; i < topCount && i < vertexHeights.size(); ++i) {
        topVertices.push_back(vertexHeights[i].second);

    }

    for (int i = 0; i < topVertices.size(); i++) {
        cout << topVertices[i].x << " " << topVertices[i].z << endl;
    }

    return topVertices;
}


void mainLoop() {

    float lastFrameTime = glfwGetTime();

    light->update();
    mat4 light_proj = light->projectionMatrix;
    mat4 light_view = light->viewMatrix;

    // Task 3.3
    // Create the depth buffer
    depth_pass(light_view, light_proj);


#ifdef PARTICLES
    vector<vec3> topVertices = findTopVertices(vertices, waves, 0, 50);  // Calculate top 50 vertices
    // Update the emitter positions

    initializeEmitters(topVertices);
#endif // PARTICLES

    

    /*auto* quad = new Drawable("quad.obj");

    FountainEmitter f_emitter = FountainEmitter(quad, 150);
    f_emitter.emitter_pos = vertices[0];
    f_emitter.use_rotations = true;
    f_emitter.use_sorting = false;
    f_emitter.height_threshold = 100.0f;
    GLuint projectionAndViewMatrix = glGetUniformLocation(particleShaderProgram, "PV");*/


    do {
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //camera->update();
        //mat4 projectionMatrix = camera->projectionMatrix;
        //mat4 viewMatrix = camera->viewMatrix;


        //

        //glDepthFunc(GL_LEQUAL);
        //glDepthMask(GL_FALSE);
        //glUseProgram(skyboxProgram);
        //mat4 viewMatrixSky = mat4(mat3(camera->viewMatrix));  // Remove the translation part

        //
        //glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE, &viewMatrixSky[0][0]);
        //glUniformMatrix4fv(glGetUniformLocation(skyboxProgram,"projection"), 1, GL_FALSE, &projectionMatrix[0][0]);


        //glBindVertexArray(skyboxVAO);
        //glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        //glBindVertexArray(0);

        //// Switch back to the normal depth function
        //glDepthMask(GL_TRUE);
        //glDepthFunc(GL_LESS);
        
        
        //glUseProgram(shaderProgram);
        light->update();
        mat4 light_proj = light->projectionMatrix;
        mat4 light_view = light->viewMatrix;
        depth_pass(light_view, light_proj);

        //// Getting camera information
        camera->update();
        mat4 projectionMatrix = camera->projectionMatrix;
        mat4 viewMatrix = camera->viewMatrix;




        lighting_pass(viewMatrix, projectionMatrix);


        ////renderFoamPass();

        //// 2. Analyze foam positions (optional, if you need the positions)

        waveUpdate();


        float currentTime = glfwGetTime();
        float dt = currentTime - lastFrameTime;
        lastFrameTime = currentTime;
#ifdef PARTICLES
        float time = glfwGetTime();
        vector<vec3> topVertices = findTopVertices(vertices, waves, time, 50);  // Calculate top 50 vertices
        

        // Initialize emitters with top vertex positions (if needed only once, move this out)
        updateEmitters(topVertices);

        // Set up the particle shader program
        glUseProgram(particleShaderProgram);
        auto PV = projectionMatrix * viewMatrix;
        glUniformMatrix4fv(projectionAndViewMatrix, 1, GL_FALSE, &PV[0][0]);

        glEnable(GL_PROGRAM_POINT_SIZE);

        // Update and render each emitter
        for (auto& emitter : emitters) {
            emitter.updateParticles(currentTime, dt, camera->position);
            emitter.renderParticles();
        }

        glDisable(GL_PROGRAM_POINT_SIZE);
#endif



        uploadLight(*light);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glUniform1i(depthMapSampler, 0);

        mat4 lightVP = light->lightVP();
        glUniformMatrix4fv(lightVPLocation, 1, GL_FALSE, &lightVP[0][0]);


        //renderDepthMap();

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
        glfwWindowShouldClose(window) == 0);
}

void initialize() {
    if (!glfwInit()) {
        throw runtime_error("Failed to initialize GLFW\n");
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(W_WIDTH, W_HEIGHT, TITLE, NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        throw runtime_error("Failed to open GLFW window.\n");
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;

    if (glewInit() != GLEW_OK) {
        glfwTerminate();
        throw runtime_error("Failed to initialize GLEW\n");
    }

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwPollEvents();
    glfwSetCursorPos(window, W_WIDTH / 2, W_HEIGHT / 2);

    glClearColor(0.4, 0.4, 0.4, 1.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    logGLParameters();

    camera = new Camera(window);
    light = new Light(window,
        vec4{ 1, 1, 1, 1 },
        vec4{ 1, 1, 1, 1 },
        vec4{ 1, 1, 1, 1 },
        vec3{ N,20, 4*N },
        50.0f
    );
}

int main(void) {
    try {
        initialize();
        createContext();
        createWaves(waveCount);
        mainLoop();
        free();
    }
    catch (exception& ex) {
        cout << ex.what() << endl;
        getchar();
        free();
        return -1;
    }
    return 0;
}