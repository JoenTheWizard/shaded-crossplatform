#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <fstream>
#include <sstream>
#include <portaudio.h>
#include <mpg123.h>

// Error checking macro
#define PA_CHECK(err) if (err != paNoError) { \
    std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl; \
    exit(1); \
}

//Global audio data for portaudio
const int AUDIO_BUFFER_SIZE = 512;
float audioData[AUDIO_BUFFER_SIZE] = {0}; //Buffer for FFT/audio texture
PaStream* audioStream;

//Audio texture for sending data to shader
GLuint audioTexture;

//mpg123 variables
mpg123_handle *mh = NULL;
unsigned char *buffer = NULL;
size_t buffer_size = 0;
size_t done = 0;
int channels, encoding;
long rate;
bool playing_file = false;

//Circular buffer for MP3 audio data
#define MP3_BUFFER_SIZE 65536  // Large buffer to prevent underruns
float mp3CircularBuffer[MP3_BUFFER_SIZE] = {0};
size_t mp3ReadPos = 0;
size_t mp3WritePos = 0;
bool mp3BufferInitialized = false;

//Audio callback for audio output
static int audioCallback(
    const void* input, void* output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData
) {
    float* out = (float*)output;

    //Only generate sine wave if we're not playing an MP3 file
    if (!playing_file) {
        static float phase         = 0.0f;
        const float freq           = 440.0f;
        const float sampleRate     = 44100.0f;
        const float phaseIncrement = freq * 2.0f * 3.14159f / sampleRate;

        for (int i = 0; i < frameCount; i++) {
            float sample = 0.1f * sin(phase); //Scale to [-0.1, 0.1] for safety
            out[i * 2] = sample;              //Left channel
            out[i * 2 + 1] = sample;          //Right channel
            phase += phaseIncrement;

            if (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;

            audioData[i % AUDIO_BUFFER_SIZE] = sample; //Update texture data
        }
    } else {
        //If we're playing an MP3, take audio from circular buffer
        for (int i = 0; i < frameCount; i++) {
            if (mp3ReadPos != mp3WritePos) { // Buffer has data
                float sample = mp3CircularBuffer[mp3ReadPos];

                //Copy sample to audio output
                out[i * 2] = sample;      // Left channel
                out[i * 2 + 1] = sample;  // Right channel

                //Also copy to visualization buffer
                audioData[i % AUDIO_BUFFER_SIZE] = sample;

                //Advance read position
                mp3ReadPos = (mp3ReadPos + 1) % MP3_BUFFER_SIZE;
            } else {
                //Buffer underrun - output silence
                out[i * 2] = 0.0f;
                out[i * 2 + 1] = 0.0f;
            }
        }
    }

    return paContinue;
}

//Initialize the audio texture
void init_audio_texture() {
    glGenTextures(1, &audioTexture);
    glBindTexture(GL_TEXTURE_1D, audioTexture);

    //Set texture parameters
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //Initialize with zeros
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, AUDIO_BUFFER_SIZE, 0, GL_RED, GL_FLOAT, audioData);

    glBindTexture(GL_TEXTURE_1D, 0);
}

//Update audio texture with current data
void update_audio_texture() {
    glBindTexture(GL_TEXTURE_1D, audioTexture);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, AUDIO_BUFFER_SIZE, GL_RED, GL_FLOAT, audioData);
    glBindTexture(GL_TEXTURE_1D, 0);
}

//Initialize PortAudio (stereo, 32-bit float)
void init_audio() {
    PaError err = Pa_Initialize();
    PA_CHECK(err);

    err = Pa_OpenDefaultStream(
        &audioStream,
        0,          //No input
        2,          //Stereo output
        paFloat32,  //32-bit float (matches OpenAL scaling)
        44100,      //Sample rate
        512,        //Larger buffer to reduce glitches
        audioCallback,
        nullptr
    );
    PA_CHECK(err);

    err = Pa_StartStream(audioStream);
    PA_CHECK(err);

    //Initialize the audio texture
    init_audio_texture();
}

//Initialize mpg123 for playing MP3 files
bool init_mp3(const char* filename) {
    int err = MPG123_OK;

    //Initialize mpg123
    if (mpg123_init() != MPG123_OK) {
        std::cerr << "Failed to initialize mpg123" << std::endl;
        return false;
    }

    //Create a new handle
    mh = mpg123_new(NULL, &err);
    if (mh == NULL) {
        std::cerr << "Failed to create mpg123 handle: " << mpg123_plain_strerror(err) << std::endl;
        return false;
    }

    //Open the file
    if (mpg123_open(mh, filename) != MPG123_OK) {
        std::cerr << "Failed to open " << filename << ": " << mpg123_strerror(mh) << std::endl;
        mpg123_delete(mh);
        return false;
    }

    //Get format information
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        std::cerr << "Failed to get format: " << mpg123_strerror(mh) << std::endl;
        mpg123_close(mh);
        mpg123_delete(mh);
        return false;
    }

    std::cout << "MP3 Format: " << rate << " Hz, " << channels << " channels" << std::endl;

    //Allocate buffer
    buffer_size = mpg123_outblock(mh);
    buffer = new unsigned char[buffer_size];

    playing_file = true;
    return true;
}

//Read and process MP3 data
void process_mp3_frame() {
    if (!playing_file || !mh) return;

    //Make sure we have some space in the circular buffer
    size_t available_space = (mp3ReadPos <= mp3WritePos) ?
                            (MP3_BUFFER_SIZE - (mp3WritePos - mp3ReadPos)) :
                            (mp3ReadPos - mp3WritePos);

    //Don't read more if we have less than 20% buffer space available
    if (available_space < MP3_BUFFER_SIZE / 5) {
        return;
    }

    //Read a frame from the MP3 file
    int result = mpg123_read(mh, buffer, buffer_size, &done);
    if (result == MPG123_OK) {
        //Convert to float and store in the circular buffer
        short* samples = (short*)buffer;
        int num_samples = done / sizeof(short);

        for (int i = 0; i < num_samples; i += channels) {
            float sample;

            if (channels == 2) {
                //Average left and right channels
                float left = samples[i] / 32768.0f;
                float right = samples[i+1] / 32768.0f;
                sample = (left + right) * 0.5f;
            }
            else {
                sample = samples[i] / 32768.0f;
            }

            //Write to circular buffer
            mp3CircularBuffer[mp3WritePos] = sample;
            mp3WritePos = (mp3WritePos + 1) % MP3_BUFFER_SIZE;

            //If buffer wasn't initialized yet, copy directly to audioData for immediate visualization
            if (!mp3BufferInitialized && i/channels < AUDIO_BUFFER_SIZE) {
                audioData[i/channels] = sample;
            }
        }

        mp3BufferInitialized = true;
    }
    else if (result == MPG123_DONE) {
        //Reached end of file
        std::cout << "End of MP3 file, looping..." << std::endl;
        mpg123_seek(mh, 0, SEEK_SET); // Loop back to beginning
    }
    else {
        std::cerr << "Error reading MP3: " << mpg123_strerror(mh) << std::endl;
    }
}

//Cleanup MP3 resources
void cleanup_mp3() {
    if (buffer) delete[] buffer;
    if (mh) {
        mpg123_close(mh);
        mpg123_delete(mh);
    }
    mpg123_exit();

    //Reset MP3 buffer state
    mp3ReadPos = 0;
    mp3WritePos = 0;
    mp3BufferInitialized = false;
}

//Framebuffer resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

//Mouse movement (iMouse)
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

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
      std::cout << "Usage: " << argv[0] << " <glsl-fragment-shader> [mp3-file]" << std::endl;
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
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); //Resize window event
    glfwSetCursorPosCallback(window, mouse_callback); //Cursor position event

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize glad with processes " << std::endl;
        exit(-1);
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    //Initialize audio
    init_audio();

    //If MP3 file is provided, initialize MP3 playback
    if (argc > 2) {
        if (!init_mp3(argv[2])) {
            std::cerr << "Failed to initialize MP3 playback" << std::endl;
        } else {
            std::cout << "Playing MP3 file: " << argv[2] << std::endl;
        }
    }

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
    checkCompileErrors(shaderProgram, "PROGRAM");

    //Remove/deallocate shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(shaderProgram);
    glUniform2fv(glGetUniformLocation(shaderProgram, "iResolution"), 1, &screen[0]);

    //Main render loop
    while (!glfwWindowShouldClose(window)) {

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Process MP3 data if playing a file
        // Process multiple frames to ensure buffer stays filled
        if (playing_file) {
            // Fill buffer with enough frames
            for (int i = 0; i < 5; i++) {
                process_mp3_frame();
            }
        }

        // Update audio texture with current audio data
        update_audio_texture();

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);


        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(shaderProgram);

        //Update uniforms
        glUniform1f(glGetUniformLocation(shaderProgram, "iTime"), currentFrame);

        //Bind audio texture to texture unit 1
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, audioTexture);
        glUniform1i(glGetUniformLocation(shaderProgram, "iAudio"), 1);

        //Also pass audio buffer size
        glUniform1i(glGetUniformLocation(shaderProgram, "iAudioSize"), AUDIO_BUFFER_SIZE);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        glfwSwapBuffers(window);
        glfwPollEvents();

    }

    //Cleanup
    Pa_StopStream(audioStream);
    Pa_CloseStream(audioStream);
    Pa_Terminate();

    if (playing_file) {
        cleanup_mp3();
    }

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

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
  float x = static_cast<float>(xpos);
  float y = static_cast<float>(ypos);

  //Set the iMouse uniform variable to actual screen x and y
  glUniform2f(glGetUniformLocation(shaderProgram, "iMouse"), x, y);
}
