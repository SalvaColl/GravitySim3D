#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <cmath>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"


const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model, view, projection;
out vec3 Normal;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    // In a sphere centered at 0,0,0, the position IS the normal!
    Normal = normalize(aPos); 
})glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
in vec3 Normal;
out vec4 FragColor;
uniform vec4 objectColor;
void main() {
    // Fake sunlight coming from top-right
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); 
    // Calculate shadow based on angle
    float diff = max(dot(Normal, lightDir), 0.2); // 0.2 is ambient light
    
    FragColor = vec4(objectColor.rgb * diff, objectColor.a);
})glsl";

// --- PHYSICS CONSTANTS AND DATA ---
const double G = 6.6743e-11; 
const double SUN_MASS = 1.989e30; 
const double EARTH_MASS = 5.972e24;
const double EARTH_RADIUS = 6'371'000.0; 
const double AU = 149'597'870'700.0;
const double MOON_RADIUS = 1'737'000.0;
const double MOON_MASS = 7.347e22;
const double LUNAR_DISTANCE = 384'400'000.0;
const double LUNAR_VELOCITY = 1'022.0;

// --- GRAPHICS SCALING ---
const double DISTANCE_SCALE = 1e7;
const double RADIUS_SCALE = 1.0;
double PLANET_RADIUS_SCALE = 8.0;
double TIME_SCALE = 100'000.0;
double TOP_VIEW_SCALE = 1.0;

// --- CAMMERA STATE --- 
int cameraState = 0; // -1 free cam, >= 0 locked
bool drawingOrbit = false;
int orbitState = -1; // -1 not drawing, >= 0 drawing
float yaw = -90.0f;
float pitch = -89.0f; // avoid 90 or -90

glm::vec3 cameraPos;
// Where the camera is looking
glm::vec3 cameraFront;
// To know which way is up and down
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float lastX = 600.0f, lastY = 600.0f;
bool firstMouse = true;           

bool running = true;
float deltaTime = 0.0;
float lastFrame = 0.0;

int fps = 83;

GLFWwindow* StartGLU();
GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource);
void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount);
glm::vec3 sphericalToCartesian(float r, float theta, float phi);
void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos);
void processInput(GLFWwindow*);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

class Object {
    public:
        // PHYSICS
        glm::dvec3 position; // Only used by the transformation matrix, objects are "drawn" at 0, 0, 0 and then moved
        glm::dvec3 velocity;
        double realRadius;
        double mass;

        // GRAPHICS
        size_t vertexCount;
        glm::vec4 color;
        float renderRadius;
        bool planet;
        std::vector<glm::vec3> trail;
        GLuint trailVAO, trailVBO;
        bool isOrbitComplete = false;

        Object(glm::dvec3 initPosition, glm::dvec3 initVelocity, double mass, double realRadius, glm::vec4 initColor, bool planet) {
            this->position = initPosition;
            this->velocity = initVelocity;
            this->mass = mass;
            this->realRadius = realRadius;
            this->color = initColor;
            this->planet = planet;
            
            // Scale the real radius down so OpenGL can draw it
            this->renderRadius = static_cast<float>(realRadius / DISTANCE_SCALE) * RADIUS_SCALE; 

            // Generate buffers for the trail
            glGenVertexArrays(1, &trailVAO);
            glGenBuffers(1, &trailVBO);

            // Bind them and set the data format
            glBindVertexArray(trailVAO);
            glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
            
            // 3 floats per point
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(0);
            
            glBindVertexArray(0);
        }

        void Accelerate(double accelX, double accelY, double accelZ, double timeStep) {
            glm::dvec3 acceleration(accelX, accelY, accelZ);
            this->velocity += acceleration * timeStep;
        }

        void UpdatePos(double timeStep) {
            this->position += this->velocity * timeStep;
        }

        void RecordTrailPoint() {
            glm::vec3 scaledPos(
                position.x / DISTANCE_SCALE,
                position.y / DISTANCE_SCALE,
                position.z / DISTANCE_SCALE
            );

            // Only add a point if we moved far enough (0.5 units)
            // This is to avoid problems with very small Time Scales
            if (trail.empty() || glm::length(scaledPos - trail.back()) > 0.5f) {
                trail.push_back(scaledPos);

                glBindVertexArray(trailVAO);
                glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
                glBufferData(GL_ARRAY_BUFFER, trail.size() * sizeof(glm::vec3), trail.data(), GL_DYNAMIC_DRAW);

                // Check if we have completed the orbit
                if (trail.size() > 100) {
                    if (glm::length(scaledPos - trail[0]) < 0.6f) {
                        isOrbitComplete = true;
                    }
                }
            }
        }

        void ClearTrail() {
            trail.clear();
            isOrbitComplete = false;
        }

        // Not used right now
        float Collisions(Object& other) {
            float dx = other.position[0] - this->position[0];
            float dy = other.position[1] - this->position[1];
            float dz = other.position[2] - this->position[2];
            float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
            if(other.realRadius + this->realRadius > distance) {
                return -1.0f;
            }
            return 1.0f;
        }
};

// Returns the vertex count so we know how many triangles to draw later
size_t GenerateUnitSphere(GLuint& VAO, GLuint& VBO) {
    std::vector<float> vertices;
    int stacks = 30;
    int sectors = 30;
    // We will only "draw" a unit sphere and then change its size with the transformation matrix
    // This way we only calculate vertices one time
    float radius = 1.0f; 

    for(float i = 0.0f; i <= stacks; ++i){
        float theta1 = (i / stacks) * glm::pi<float>();
        float theta2 = (i+1) / stacks * glm::pi<float>();
        for (float j = 0.0f; j < sectors; ++j){
            float phi1 = j / sectors * 2 * glm::pi<float>();
            float phi2 = (j+1) / sectors * 2 * glm::pi<float>();
            
            glm::vec3 v1 = sphericalToCartesian(radius, theta1, phi1);
            glm::vec3 v2 = sphericalToCartesian(radius, theta1, phi2);
            glm::vec3 v3 = sphericalToCartesian(radius, theta2, phi1);
            glm::vec3 v4 = sphericalToCartesian(radius, theta2, phi2);

            vertices.insert(vertices.end(), {v1.x, v1.y, v1.z}); 
            vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
            vertices.insert(vertices.end(), {v3.x, v3.y, v3.z}); 
            
            vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
            vertices.insert(vertices.end(), {v4.x, v4.y, v4.z});
            vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
        }   
    }
    
    CreateVBOVAO(VAO, VBO, vertices.data(), vertices.size());
    return vertices.size();
}

std::vector<Object> objs = {};
const std::string bodyNames[] = {"SUN", "MERCURY", "VENUS", "EARTH", "MOON"};

int main() {
    GLFWwindow* window = StartGLU();
    GLuint shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUseProgram(shaderProgram);

    
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io; 

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 750000.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    objs = {
        // SUN
        Object(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(0.0, 0.0, 0.0), SUN_MASS, 696340000.0, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), false),
        
        // MERCURY
        Object(glm::dvec3(0.387 * AU, 0.0, 0.0), glm::dvec3(0.0, 0.0, 47360.0), 3.301e23, 2439700.0, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), true),
        
        // VENUS
        Object(glm::dvec3(0.723 * AU, 0.0, 0.0), glm::dvec3(0.0, 0.0, 35020.0), 4.867e24, 6051800.0, glm::vec4(1.0f, 0.7f, 0.3f, 1.0f), true),

        // EARTH:
        Object(glm::dvec3(AU, 0.0, 0.0), glm::dvec3(0.0, 0.0, 29780.0), EARTH_MASS, EARTH_RADIUS, glm::vec4(0.0f, 0.5f, 1.0f, 1.0f), true),

        // MOON:
        Object(glm::dvec3(AU + LUNAR_DISTANCE, 0.0, 0.0), glm::dvec3(0.0, 0.0, 29780.0 + LUNAR_VELOCITY), MOON_MASS, MOON_RADIUS, glm::vec4(0.7f, 0.7f, 0.7f, 1.0f), true)
    };

    float previousTime = glfwGetTime();
    int frameCount = 0;

    GLuint sphereVAO, sphereVBO;
    size_t sphereVertexCount = GenerateUnitSphere(sphereVAO, sphereVBO);

    while (!glfwWindowShouldClose(window) && running == true) {
        processInput(window);

        // Display coordinates
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Once);
        ImGui::Begin("Simulation Data");

        ImGui::SetWindowFontScale(1.5f); 

        ImGui::Text("Camera Coordinates:");
        ImGui::Text("X: %.1f", cameraPos.x);
        ImGui::Text("Y: %.1f", cameraPos.y);
        ImGui::Text("Z: %.1f", cameraPos.z);

        ImGui::End();

        // FPS
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        frameCount++;
        if(currentFrame - previousTime >= 1.0) {
            std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE) + " | PLANET SCALE: " + std::to_string(PLANET_RADIUS_SCALE);
            if(cameraState != -1) {
                title += + " | CURRENT BODY: " + bodyNames[cameraState/2];
            }        
            glfwSetWindowTitle(window, title.c_str());
            fps = frameCount;
            frameCount = 0;
            previousTime = currentFrame;
        }

        // Gravity
        for(size_t i = 0; i < objs.size(); i++) {
            glm::dvec3 totalAcceleration(0.0);
            // Calculate total gravitational pull
            for(size_t j = 0; j < objs.size(); j++) {
                if(i != j) {
                    glm::dvec3 diff = objs[j].position - objs[i].position;
                    double distance = glm::length(diff);
                    
                    if (distance > 0.1) { // Avoid division by 0
                        glm::dvec3 direction = glm::normalize(diff);
                        // F = G*m1*m2/r^2
                        double force = (G * objs[i].mass * objs[j].mass) / (distance * distance);
                        double accel = force / objs[i].mass;
                        totalAcceleration += direction * accel;
                    }
                }
            }

            double timeStep = deltaTime * TIME_SCALE; 
            objs[i].Accelerate(totalAcceleration.x, totalAcceleration.y, totalAcceleration.z, timeStep);
            objs[i].UpdatePos(timeStep);

            if (drawingOrbit && orbitState == i && !objs[i].isOrbitComplete) {
                objs[i].RecordTrailPoint();
            }
        }

        // --- CAMERA TRACKING ---
        if (cameraState != -1) {
            int targetIdx = cameraState / 2;      
            bool isTopView = (cameraState % 2 == 0); 
        
            glm::vec3 targetPos = glm::vec3(
                objs[targetIdx].position.x / DISTANCE_SCALE,
                objs[targetIdx].position.y / DISTANCE_SCALE,
                objs[targetIdx].position.z / DISTANCE_SCALE
            );
            
            // Actual render size right now
            float currentVisualRadius = objs[targetIdx].renderRadius;
            if (objs[targetIdx].planet) currentVisualRadius *= PLANET_RADIUS_SCALE;

            if (isTopView) {
                float viewDist;
                if (targetIdx == 0) viewDist = (AU / DISTANCE_SCALE) * 3.0f; // Sun Top View
                else if (targetIdx == 1) viewDist = (LUNAR_DISTANCE / DISTANCE_SCALE) * 5.0f; // Earth Top View
                else viewDist = (LUNAR_DISTANCE / DISTANCE_SCALE) * 5.0f; // Moon Top View
                
                cameraPos = targetPos + glm::vec3(0.0f, viewDist, 0.0f);
            } else {
                // Side View for any object
                cameraPos = targetPos + glm::vec3(0.0f, 0.0f, currentVisualRadius * 3.0f);
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        UpdateCam(shaderProgram, cameraPos);
        glBindVertexArray(sphereVAO);
        
        // Draw the triangles / sphere
        for(auto& obj : objs) {
            glUniform4f(objectColorLoc, obj.color.r, obj.color.g, obj.color.b, obj.color.a);

            // Scale down for drawing
            glm::vec3 scaledPosition(
                obj.position.x / DISTANCE_SCALE,
                obj.position.y / DISTANCE_SCALE,
                obj.position.z / DISTANCE_SCALE
            );

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, scaledPosition); 

            float finalScale = obj.renderRadius;
            if(obj.planet) finalScale *= PLANET_RADIUS_SCALE;
            
            model = glm::scale(model, glm::vec3(finalScale));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            
            // Draw the currently bound VAO
            glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount / 3);
        }

        if (drawingOrbit && orbitState != -1 && objs[orbitState].trail.size() >= 2) {
            glUniform4f(objectColorLoc, objs[orbitState].color.r, objs[orbitState].color.g, objs[orbitState].color.b, objs[orbitState].color.a);
            
            glm::mat4 model = glm::mat4(1.0f);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            glBindVertexArray(objs[orbitState].trailVAO);
            glDrawArrays(GL_LINE_STRIP, 0, objs[orbitState].trail.size());
        }
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);

    for (auto& obj : objs) {
        glDeleteVertexArrays(1, &obj.trailVAO);
        glDeleteBuffers(1, &obj.trailVBO);
    }

    glDeleteProgram(shaderProgram);
    glfwTerminate();

    glfwTerminate();
    return 0;
}

GLFWwindow* StartGLU() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW, panic" << std::endl;
        return nullptr;
    }
    GLFWwindow* window = glfwCreateWindow(1200, 1200, "3D_TEST", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(2); // 1 = VSync ON. 0 = VSync OFF.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return nullptr;
    }

    glEnable(GL_DEPTH_TEST);    
    glViewport(0, 0, 1200, 1200);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return window;
}

GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
    }

    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    // Shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

glm::vec3 sphericalToCartesian(float r, float theta, float phi){
    float y = r * cos(theta);
    // r * sin(theta) is the radius of the current disk of the sphere
    float x = r * sin(theta) * cos(phi);
    float z = r * sin(theta) * sin(phi);
    return glm::vec3(x, y, z);
};

void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos) {
    glUseProgram(shaderProgram);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void processInput(GLFWwindow* window) {
    float cameraSpeed = 1250.0f * deltaTime;
    // Continuous inputs, check what is happening right now
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        cameraSpeed *= 5.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cameraPos += cameraSpeed * cameraFront;
        cameraState = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * cameraFront;
        cameraState = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
        cameraState = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cameraPos += cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
        cameraState = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        cameraPos += cameraSpeed * cameraUp;
        cameraState = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * cameraUp;
        cameraState = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS){
        glfwSetWindowShouldClose(window, true);
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // One action inputs

    // Time Scale
    if (key == GLFW_KEY_UP && action == GLFW_PRESS) {
        TIME_SCALE += 100000;
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE) + " | PLANET SCALE: " + std::to_string(PLANET_RADIUS_SCALE);
        if(cameraState != -1) {
            title += + " | CURRENT BODY: " + bodyNames[cameraState/2];
        }
        glfwSetWindowTitle(window, title.c_str());
    }
    if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
        TIME_SCALE -= 100000;
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE) + " | PLANET SCALE: " + std::to_string(PLANET_RADIUS_SCALE);
        if(cameraState != -1) {
            title += + " | CURRENT BODY: " + bodyNames[cameraState/2];
        }
        glfwSetWindowTitle(window, title.c_str());
    }

    // Planet Radius Scale
    if (key == GLFW_KEY_9 && action == GLFW_PRESS) {
        PLANET_RADIUS_SCALE *= 2.0;
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE) + " | PLANET SCALE: " + std::to_string(PLANET_RADIUS_SCALE);
        if(cameraState != -1) {
            title += + " | CURRENT BODY: " + bodyNames[cameraState/2];
        }        
        glfwSetWindowTitle(window, title.c_str());
    }
    if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
        PLANET_RADIUS_SCALE /= 2.0;
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE) + " | PLANET SCALE: " + std::to_string(PLANET_RADIUS_SCALE);
        if(cameraState != -1) {
            title += + " | CURRENT BODY: " + bodyNames[cameraState/2];
        }        
        glfwSetWindowTitle(window, title.c_str());
    }

    // Camera State 
    if ((key == GLFW_KEY_RIGHT || key == GLFW_KEY_LEFT) && action == GLFW_PRESS) {
        if (key == GLFW_KEY_RIGHT) {
            if (cameraState == -1) cameraState = 0;
            else cameraState = (cameraState + 1) % 10; 
        } else {
            if (cameraState == -1) cameraState = 9;
            else cameraState = (cameraState - 1 + 10) % 10; 
        }

        bool isTopView = (cameraState % 2 == 0);
        
        yaw = -90.0f;
        pitch = isTopView ? -89.0f : 0.0f; 

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE) + " | PLANET SCALE: " + std::to_string(PLANET_RADIUS_SCALE);
        if(cameraState != -1) {
            title += + " | CURRENT BODY: " + bodyNames[cameraState/2];
        }        
        glfwSetWindowTitle(window, title.c_str());
    }

    // Orbit Drawing
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        if (cameraState != -1) {
            int targetIdx = cameraState / 2;
            
            // If we press R on the planet we are already drawing, toggle it OFF and erase
            if (drawingOrbit && orbitState == targetIdx) {
                drawingOrbit = false;
                objs[targetIdx].ClearTrail();
            } else {
                // If we press R on a NEW planet, clear the old one and start drawing
                if (orbitState != -1) objs[orbitState].ClearTrail();
                drawingOrbit = true;
                orbitState = targetIdx;
                objs[targetIdx].ClearTrail();
            }
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    float cameraSpeed = 50000.0f * deltaTime;
    if(yoffset>0){
        cameraPos += cameraSpeed * cameraFront;
    } else if(yoffset<0){
        cameraPos -= cameraSpeed * cameraFront;
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if(pitch > 89.0f) pitch = 89.0f;
    if(pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}