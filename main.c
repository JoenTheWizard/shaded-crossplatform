#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <fstream>
#include <sstream>

//Framebuffer resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

//Method to read file
int read_file(const char* filePath, std::string& fileString);

//For obtaining any errors from the shader program
void checkCompileErrors(unsigned int shader, std::string type);

//Main fragment shader ID
GLuint shaderProgram;

//Vertex shader of the rendering quad
const char *vertexShaderSource = 
R"(#version 330 core
layout (location = 0) in vec2 position;            
layout (location = 1) in vec2 inTexCoord;

out vec2 texCoord;
void main(){
    texCoord = inTexCoord;
    gl_Position = vec4(position.x, position.y, 0.0f, 1.0f);
})";

int main(int argc, char** argv) {
    int width = 200;
    int height = 200;

    glm::vec2 screen(width, height);

    float deltaTime = 0.0f; 
    float lastFrame = 0.0f; 

    //Check if necessary arguments are passed in
    if (argc < 2) {
      std::cout << "Usage: " << argv[0] << " <glsl-fragment-shader>" << std::endl; 
      return -1;
    }

    //First get fragment shader code from file
    std::string fragmentShaderCode;
    if (read_file(argv[1], fragmentShaderCode)) {
      return -1;
    }
    //Obtain the fragment shader code to be passed in the shader compilation
    const char* fragCode = fragmentShaderCode.c_str();

    //Initialize GLFW window context
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(width, height, "ShadeD", nullptr, nullptr);
    if (!window) {
        std::cerr << "failed to create window" << std::endl;
        exit(-1);
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize glad with processes " << std::endl;
        exit(-1);
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    int samples = 4;
    float quadVerts[] = {
       //Position       //UV
       -1.0, -1.0,      0.0, 0.0,
       -1.0,  1.0,      0.0, 1.0,
        1.0, -1.0,      1.0, 0.0,

        1.0, -1.0,      1.0, 0.0,
       -1.0,  1.0,      0.0, 1.0,
        1.0,  1.0,      1.0, 1.0
    };

    //Initialize the quad VAO and VBO for the screen rendering
    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    //Remove/deallocate shaders
    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    //Create a framebuffer texture
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer); 

    //Texture for the framebuffer
    GLuint texColor;
    glGenTextures(1, &texColor);
    glBindTexture(GL_TEXTURE_2D, texColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColor, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    //Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragCode, NULL);
    glCompileShader(fragmentShader);
    //Check for shader compile errors
    checkCompileErrors(fragmentShader, "FRAGMENT");

    //Link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    //Check for program linking errors
    //checkCompileErrors(shaderProgram, "PROGRAM");

    //Remove/deallocate shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(shaderProgram);
    glUniform2fv(glGetUniformLocation(shaderProgram, "iResolution"), 1, &screen[0]);


    while (!glfwWindowShouldClose(window)) {

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame; 

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);


        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(shaderProgram);
        glUniform1f(glGetUniformLocation(shaderProgram, "iTime"), currentFrame); 
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        glfwSwapBuffers(window);
        glfwPollEvents();

    }

    //Cleanup
    glfwTerminate();
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    //Make sure the viewport matches the new window dimensions; note that width and 
    //height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
 
    //Update the iResolution uniform in the shader
    glUniform2fv(glGetUniformLocation(shaderProgram, "iResolution"), 1, &glm::vec2(width, height)[0]);
}

int read_file(const char* filePath, std::string& fileString) {
    std::string fragmentCode;
    std::ifstream fShaderFile;

    fShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);

    try {
      //First attempt to read the file
      fShaderFile.open(filePath);
      std::stringstream fShaderStream;

      //Read file's buffer contents into streams
      fShaderStream << fShaderFile.rdbuf();

      //Close
      fShaderFile.close();

      //Stream into string
      fileString = fShaderStream.str();

    }
    catch (std::ifstream::failure& e) {
      std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
      return -1;
    }

    return 0;
}

//Check the compile errors
void checkCompileErrors(unsigned int shader, std::string type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
        }
    }
}
