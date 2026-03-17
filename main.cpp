#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <cmath>


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

const double G = 6.6743e-11; // Gravitational constant
const double SUN_MASS = 1.989e30; 
const double EARTH_MASS = 5.972e24;
const double EARTH_RADIUS = 6371000.0; // In meters
const double AU = 149597870700.0;
const double MOON_RADIUS = 1737000.0;
const double MOON_MASS = 7.347e22;
const double LUNAR_DISTANCE = 384400000.0;
const double LUNAR_VELOCITY = 1022.0;

// --- GRAPHICS SCALING ---
// 1 unit in OpenGL = 10,000,000 meters in real life.
const double DISTANCE_SCALE = 1e7;
const double RADIUS_SCALE = 1.0;
const double PLANET_RADIUS_SCALE = 10.0;
double TIME_SCALE = 100000.0;

glm::vec3 cameraPos;
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);
float lastX = 600.0f, lastY = 600.0f;
bool firstMouse = true;           
float yaw = -90;
float pitch = 0.0;
int lockedTarget = 0; // -1 free cam, 0 Sun, 1 Earth

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
        GLuint VAO, VBO;
        // PHYSICS
        // Only used by the transformation matrix
        // So always draw around 0, 0, 0
        glm::dvec3 position;
        glm::dvec3 velocity;
        double realRadius;
        double mass;

        // GRAPHICS
        size_t vertexCount;
        glm::vec4 color;
        float renderRadius;
        

        Object(glm::dvec3 initPosition, glm::dvec3 initVelocity, double mass, double realRadius, glm::vec4 initColor, bool planet) {
            this->position = initPosition;
            this->velocity = initVelocity;
            this->mass = mass;
            this->realRadius = realRadius;
            this->color = initColor;
            
            // Scale the real radius down so OpenGL can draw it
            this->renderRadius = static_cast<float>(realRadius / DISTANCE_SCALE) * RADIUS_SCALE; 
            if(planet) this->renderRadius *= PLANET_RADIUS_SCALE;
            std::vector<float> vertices = Draw();
            vertexCount = vertices.size();
            CreateVBOVAO(VAO, VBO, vertices.data(), vertexCount);
        }

        /*std::vector<float> Draw() {
            std::vector<float> vertices;

            float centerX = this->position[0];
            float centerY = this->position[1];

            float leftX = -cos(M_PI/6.0)*this->radius;
            float leftY = -sin(M_PI/6.0)*this->radius;

            float rightX = cos(M_PI/6.0)*this->radius;
            float rightY = leftY;

            float topX = 0.0f;
            float topY = 0.0f + this->radius;

            vertices.insert(vertices.end(), {leftX, leftY, 0.0f});
            vertices.insert(vertices.end(), {rightX, rightY, 0.0f});
            vertices.insert(vertices.end(), {topX, topY, 0.0f});
            return vertices;
        }*/

        std::vector<float> Draw() {
            std::vector<float> vertices;
            int stacks = 50;
            int sectors = 50;

            // generate circumference points using integer steps
            for(float i = 0.0f; i <= stacks; ++i){
                float theta1 = (i / stacks) * glm::pi<float>();
                float theta2 = (i+1) / stacks * glm::pi<float>();
                for (float j = 0.0f; j < sectors; ++j){
                    float phi1 = j / sectors * 2 * glm::pi<float>();
                    float phi2 = (j+1) / sectors * 2 * glm::pi<float>();
                    glm::vec3 v1 = sphericalToCartesian(this->renderRadius, theta1, phi1);
                    glm::vec3 v2 = sphericalToCartesian(this->renderRadius, theta1, phi2);
                    glm::vec3 v3 = sphericalToCartesian(this->renderRadius, theta2, phi1);
                    glm::vec3 v4 = sphericalToCartesian(this->renderRadius, theta2, phi2);

                    // Triangle 1: v1-v2-v3
                    vertices.insert(vertices.end(), {v1.x, v1.y, v1.z}); 
                    vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                    vertices.insert(vertices.end(), {v3.x, v3.y, v3.z}); 
                    
                    // Triangle 2: v2-v4-v3
                    vertices.insert(vertices.end(), {v2.x, v2.y, v2.z});
                    vertices.insert(vertices.end(), {v4.x, v4.y, v4.z});
                    vertices.insert(vertices.end(), {v3.x, v3.y, v3.z});
                }   
            }
            return vertices;
        }

        void Accelerate(double accelX, double accelY, double accelZ, double timeStep) {
            glm::dvec3 acceleration(accelX, accelY, accelZ);
            this->velocity += acceleration * timeStep;
        }

        void UpdatePos(double timeStep) {
            this->position += this->velocity * timeStep;
        }

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

std::vector<Object> objs = {};

GLuint gridVAO, gridVBO;

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

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 750000.0f);
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);

    /*objs = {
        Object(glm::dvec3(0,0,0), glm::vec3(0,0,0), 10.0f, 0.40f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)),
        Object(glm::dvec3(1,0,0), glm::dvec3(0,0,0), 10.0f, 0.40f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f))
    };*/

    objs = {
        // SUN: Center (0,0,0), Zero velocity, Sun Mass, Sun Radius, Yellow color, Scale
        Object(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(0.0, 0.0, 0.0), SUN_MASS, 696340000.0, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), false),
        
        // EARTH: 1 AU away on X-axis, orbital velocity on Z-axis, Earth Mass, Earth Radius, Blue color, Scale
        // Earth's orbital velocity is roughly 29,780 m/s
        Object(glm::dvec3(AU, 0.0, 0.0), glm::dvec3(0.0, 0.0, 29780.0), EARTH_MASS, EARTH_RADIUS, glm::vec4(0.0f, 0.5f, 1.0f, 1.0f), true),

        // MOON:
        // Position: Earth's X + Lunar Distance. 
        // Velocity: Earth's Z + Lunar Velocity.
        Object(glm::dvec3(AU + LUNAR_DISTANCE, 0.0, 0.0), glm::dvec3(0.0, 0.0, 29780.0 + LUNAR_VELOCITY), MOON_MASS, MOON_RADIUS, glm::vec4(0.7f, 0.7f, 0.7f, 1.0f), true)
    };

    float previousTime = glfwGetTime();
    int frameCount = 0;

    while (!glfwWindowShouldClose(window) && running == true) {
        processInput(window);
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        frameCount++;
        if(currentFrame - previousTime >= 1.0) {
            std::string title = "GRAVITY SIM | FPS: " + std::to_string(frameCount) + " | TIME SCALE: " + std::to_string(TIME_SCALE);
            glfwSetWindowTitle(window, title.c_str());
            fps = frameCount;
            frameCount = 0;
            previousTime = currentFrame;
        }

        // --- CAMERA TRACKING ---
        if (lockedTarget != -1) {
            // Get the target's current scaled position
            glm::vec3 targetPos = glm::vec3(
                objs[lockedTarget].position.x / DISTANCE_SCALE,
                objs[lockedTarget].position.y / DISTANCE_SCALE,
                objs[lockedTarget].position.z / DISTANCE_SCALE
            );
            
            if (lockedTarget == 1) {
                // Top-down Earth View: 3x Lunar Distance on the Y-Axis
                float moonViewDist = (LUNAR_DISTANCE / DISTANCE_SCALE) * 3.0f;
                cameraPos = targetPos + glm::vec3(0.0f, moonViewDist, 0.0f);
            } else {
                // Sun view (or anything else)
                cameraPos = targetPos + glm::vec3(0.0f, 0.0f, objs[lockedTarget].renderRadius * 3.0f);
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        UpdateCam(shaderProgram, cameraPos);
        // Draw the triangles / sphere
        for(auto& obj : objs) {
            glUniform4f(objectColorLoc, obj.color.r, obj.color.g, obj.color.b, obj.color.a);

            // 1. Calculate the total gravitational pull on this object
            glm::dvec3 totalAcceleration(0.0);
            
            for(auto& obj2 : objs) {
                if(&obj != &obj2) {
                    glm::dvec3 diff = obj2.position - obj.position;
                    double distance = glm::length(diff);
                    
                    if (distance > 0.1) { // Prevent division by zero
                        glm::dvec3 direction = glm::normalize(diff);
                        // F = G(m1 * m2) / r^2
                        double force = (G * obj.mass * obj2.mass) / (distance * distance);
                        double accel = force / obj.mass;
                        totalAcceleration += direction * accel;
                    }
                }
            }

            double timeStep = deltaTime * TIME_SCALE; // Speed up time
            obj.Accelerate(totalAcceleration.x, totalAcceleration.y, totalAcceleration.z, timeStep);
            obj.UpdatePos(timeStep);

            // 2. Scale down for OpenGL drawing
            glm::vec3 scaledPosition(
                obj.position.x / DISTANCE_SCALE,
                obj.position.y / DISTANCE_SCALE,
                obj.position.z / DISTANCE_SCALE
            );

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, scaledPosition); 
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            
            glBindVertexArray(obj.VAO);
            glDrawArrays(GL_TRIANGLES, 0, obj.vertexCount / 3);
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto& obj : objs) {
        glDeleteVertexArrays(1, &obj.VAO);
        glDeleteBuffers(1, &obj.VBO);
    }

    glDeleteVertexArrays(1, &gridVAO);
    glDeleteBuffers(1, &gridVBO);

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
    // r * sin(theta) is the radius of the disk
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
    
    // Continuous inputs, check what is happening right now and act
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cameraPos += cameraSpeed * cameraFront;
        lockedTarget = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * cameraFront;
        lockedTarget = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
        lockedTarget = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cameraPos += cameraSpeed * glm::normalize(glm::cross(cameraFront, cameraUp));
        lockedTarget = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        cameraPos += cameraSpeed * cameraUp;
        lockedTarget = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * cameraUp;
        lockedTarget = -1;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS){
        glfwSetWindowShouldClose(window, true);
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // PRESSING ONE TIME
    
    /*if(key == GLFW_KEY_F && action == GLFW_PRESS) {
        static float spawnOffset = 1.0f;
        Object newSphere = Object(glm::vec3(spawnOffset, 0, 0), glm::vec3(0, 0, 0), 0.40f, 10.0f);
        objs.emplace_back(newSphere);
        spawnOffset += 1.0f;
    }*/

    if(key == GLFW_KEY_E && action == GLFW_PRESS) {
        lockedTarget = 1; // Lock to Earth
        
        yaw = -90.0f;
        pitch = -89.0f; // Look almost straight down to prevent Gimbal Lock!
        
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }

    if(key == GLFW_KEY_C && action == GLFW_PRESS) {
        lockedTarget = 0; // Lock to Sun
        
        yaw = -90.0f;
        pitch = 0.0f;
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }

    if(key == GLFW_KEY_Z && action == GLFW_PRESS) {
        lockedTarget = -1;
        cameraPos = glm::vec3(0.0f, AU/DISTANCE_SCALE * 3.0f, 0.0f);
        yaw = -90.0f;
        pitch = -90.0f;
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }

    if (key == GLFW_KEY_UP && action == GLFW_PRESS) {
        TIME_SCALE += 10000;
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE);
        glfwSetWindowTitle(window, title.c_str());
    }
    if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
        TIME_SCALE -= 10000;
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE);
        glfwSetWindowTitle(window, title.c_str());
    }
    if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
        TIME_SCALE += 100000;
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE);
        glfwSetWindowTitle(window, title.c_str());
    }
    if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
        TIME_SCALE -= 100000;
        std::string title = "GRAVITY SIM | FPS: " + std::to_string(fps) + " | TIME SCALE: " + std::to_string(TIME_SCALE);
        glfwSetWindowTitle(window, title.c_str());
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    float cameraSpeed = 5000.0f * deltaTime;
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