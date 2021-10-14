// gui
#include "gui/imgui.h"
#include "gui/imgui_impl_glfw.h"
#include "gui/imgui_impl_opengl3.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
// 在gl和glfw3之前包含glew
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glut.h>  // 必须添加glut，不然绘制函数不识别!!!!!!!!!!!
// glm
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "common/quaternion_utils.hpp"
#include "common/loadShader.h" // 加载着色器
#include "common/BMPlib.h"     // BMP格式读取
#include "common/texture.hpp"  // DDS格式纹理解析

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

using namespace glm;  // 直接使用glm库数据类型无需前缀
using namespace BMPlib;

static const int window_width = 1024;
static const int window_height = 768;
static const float window_ratio = window_width / (float)window_height;
static const float MAX_FOV = 90.0f;

static glm::vec3 position = glm::vec3(0, 40, 0);  // 初始相机位置
static float horizontalAngle = glm::radians(90.0f);  // 水平向左90°
static float verticalAngle = glm::radians(-90.0f);  // 垂直向下90°
static float FoV = 45.0f;
static float speed_wasd = 10.0f;  // 10 units/s
static float speed_mouse = 0.001f;
static float speed_rotate = 60.0f;
static int display_mode = GL_FILL;
static int control_mode = 1;  // 0: ↑↓←→沿坐标轴旋转 鼠标右键拉近视野 ED拉近视野
                              // 1：wasd前后左右移动 鼠标旋转视角
static int flag_display_mode = 0;
static int flag_control_mode = 0;

//static glm::mat4 rotation = glm::mat4(1.0);
//static glm::mat4 translation = glm::mat4(1.0);
//static glm::mat4 scaling = glm::mat4(1.0);
static glm::mat4 Model = glm::mat4(1.0);
static glm::mat4 Projection;
static glm::mat4 View;
static glm::mat4 MVP;


static float CarmackSqrt(float x)
{
    float xhalf = 0.5f * x;

    int i = *(int*)&x;           // get bits for floating VALUE 
    i = 0x5f3759df - (i >> 1);     // gives initial guess y0
    x = *(float*)&i;             // convert bits BACK to float
    x = x * (1.5f - xhalf * x * x);    // Newton step, repeating increases accuracy
    x = x * (1.5f - xhalf * x * x);    // Newton step, repeating increases accuracy
    x = x * (1.5f - xhalf * x * x);    // Newton step, repeating increases accuracy
    return (1 / x);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    //FoV -= 5 * yoffset;

    if (FoV >= 1.0f && FoV <= MAX_FOV) {
        FoV -= 5 * yoffset;
    }
    //initialFoV = initialFoV - 5 * yoffset;
    FoV = FoV <= 1.0f ? 1.0f : FoV;
    FoV = FoV >= MAX_FOV ? MAX_FOV : FoV;
}

GLuint load_BMP_texture(const char* imagepath) {
    // 纹理data
    BMP bmp;
    bmp.Read(imagepath);
    int width = bmp.GetWidth();
    int height = bmp.GetHeight();
    byte* data = bmp.GetPixelBuffer();

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
    // 已将data拷贝到opengl，最早可在此处释放空间:bmp.~BMP()  或等该函数结束自动释放
    //// 纹理过滤（低质量）
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // 纹理过滤（高质量:三重线性过滤）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // 生成mipmap
    glGenerateMipmap(GL_TEXTURE_2D);
    return textureID;
}

int main()
{
    // 初始化GLFW
    if (!glfwInit()){
        fprintf(stderr, "Failed to initialize GLFW\n");
        getchar();
        return -1;
    }
    glfwWindowHint(GLFW_SAMPLES, 4); // 4倍抗锯齿
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // OpenGL 3.3 版本
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    //glfwWindowHint(GLFW_CONTEXT_PROFILE, GLFW_OPENGL_CORE_PROFILE); //We don't want the old OpenGL
    // 初始化窗口与OpenGL context
    GLFWwindow* window;
    window = glfwCreateWindow(window_width, window_height, "3D Terrain", NULL, NULL);  // 窗口尺寸与标题
    if (window == NULL){
        fprintf(stderr, "Failed to open GLFW window\n");
        getchar();
        glfwTerminate();
        return -1;
    }
    // 初始化 GLEW
    glfwMakeContextCurrent(window);
    glewExperimental = true; // Needed in core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return -1;
    }
    // 捕捉键盘事件
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    // Hide the mouse and enable unlimited mouvement
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // 鼠标滚轮回调
    // Set the mouse at the center of the screen
    glfwPollEvents();
    glfwSetCursorPos(window, window_width / 2, window_height / 2);
    glfwSetScrollCallback(window, scroll_callback);
    // 背景颜色：深蓝
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    // 创建一个VAO
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    // 编译着色器程序
    GLuint programID = LoadShaders("shader\\vertexshader.glsl", "shader\\fragmentshader.glsl");
    // 深度检测
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    // Cull triangles which normal is not towards the camera 背面剔除
    glEnable(GL_CULL_FACE);

    // GUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    bool show_demo_window = true;
    bool show_another_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // 数据
    // 顶点：三角形
    /*static const GLfloat g_vertex_buffer_data[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        0.0f,  1.0f, 0.0f,
    };*/
    // 顶点颜色（静态）
    /*static const GLfloat g_color_buffer_data[] = {
        0.583f,  0.771f,  0.014f,
        0.609f,  0.115f,  0.436f,
        0.327f,  0.483f,  0.844f,
        0.822f,  0.569f,  0.201f,
        0.435f,  0.602f,  0.223f,
        0.310f,  0.747f,  0.185f,
        0.597f,  0.770f,  0.761f,
        0.559f,  0.436f,  0.730f,
        0.359f,  0.583f,  0.152f,
        0.483f,  0.596f,  0.789f,
        0.559f,  0.861f,  0.639f,
        0.195f,  0.548f,  0.859f,
        0.014f,  0.184f,  0.576f,
        0.771f,  0.328f,  0.970f,
        0.406f,  0.615f,  0.116f,
        0.676f,  0.977f,  0.133f,
        0.971f,  0.572f,  0.833f,
        0.140f,  0.616f,  0.489f,
        0.997f,  0.513f,  0.064f,
        0.945f,  0.719f,  0.592f,
        0.543f,  0.021f,  0.978f,
        0.279f,  0.317f,  0.505f,
        0.167f,  0.620f,  0.077f,
        0.347f,  0.857f,  0.137f,
        0.055f,  0.953f,  0.042f,
        0.714f,  0.505f,  0.345f,
        0.783f,  0.290f,  0.734f,
        0.722f,  0.645f,  0.174f,
        0.302f,  0.455f,  0.848f,
        0.225f,  0.587f,  0.040f,
        0.517f,  0.713f,  0.338f,
        0.053f,  0.959f,  0.120f,
        0.393f,  0.621f,  0.362f,
        0.673f,  0.211f,  0.457f,
        0.820f,  0.883f,  0.371f,
        0.982f,  0.099f,  0.879f
    };*/
    // 顶点颜色（动态）
    /*GLfloat g_color_buffer_data[12 * 3 * 3];
    for (int v = 0; v < 12 * 3; v++) {
        g_color_buffer_data[3 * v + 0] = abs(g_vertex_buffer_data[3 * v + 0] + g_vertex_buffer_data[3 * v + 1] + g_vertex_buffer_data[3 * v + 2])/3.0f * rand() / double(RAND_MAX);
        g_color_buffer_data[3 * v + 1] = abs(g_vertex_buffer_data[3 * v + 0] + g_vertex_buffer_data[3 * v + 1] + g_vertex_buffer_data[3 * v + 2])/3.0f * rand() / double(RAND_MAX);
        g_color_buffer_data[3 * v + 2] = abs(g_vertex_buffer_data[3 * v + 0] + g_vertex_buffer_data[3 * v + 1] + g_vertex_buffer_data[3 * v + 2])/3.0f * rand() / double(RAND_MAX);
    }*/
    // 顶点：正方体
    /*static const GLfloat g_vertex_buffer_data[] = {
        -1.0f,-1.0f,-1.0f, // triangle 1 : begin
        -1.0f,-1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f, // triangle 1 : end
        1.0f, 1.0f,-1.0f, // triangle 2 : begin
        -1.0f,-1.0f,-1.0f,
        -1.0f, 1.0f,-1.0f, // triangle 2 : end
        1.0f,-1.0f, 1.0f,
        -1.0f,-1.0f,-1.0f,
        1.0f,-1.0f,-1.0f,
        1.0f, 1.0f,-1.0f,
        1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f,-1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,
        1.0f,-1.0f, 1.0f,
        -1.0f,-1.0f, 1.0f,
        -1.0f,-1.0f,-1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f, 1.0f,
        1.0f,-1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f,-1.0f,-1.0f,
        1.0f, 1.0f,-1.0f,
        1.0f,-1.0f,-1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f,-1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f,-1.0f,
        -1.0f, 1.0f,-1.0f,
        1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,
        -1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        1.0f,-1.0f, 1.0f
    };*/
    // 顶点：正方形
    /*std::vector<glm::vec3> g_vertex_buffer_data;
        g_vertex_buffer_data.push_back(vec3(-1.0f, -1.0f, 0.0f));
        g_vertex_buffer_data.push_back(vec3(1.0f, -1.0f, 0.0f));
        g_vertex_buffer_data.push_back(vec3(1.0f, 1.0f, 0.0f));
        g_vertex_buffer_data.push_back(vec3(-1.0f, 1.0f, 0.0f));
    */
    // 顶点：BMP地形
    // 加载BMP，读取地形尺寸、地形高度
    BMP bmp;
    bmp.Read("res/terrain.bmp");
    bmp.ConvertTo(BMP::COLOR_MODE::BW, true);
    const int width = bmp.GetWidth();
    const int height = bmp.GetHeight();
    const float model_w = width * 0.1f;
    const float model_h = height * 0.1f;
    const float model_t = 255 / 255.0f;
    const float model_d = CarmackSqrt((width * width) + (height * height)) * 0.1f;
    byte* data = bmp.GetPixelBuffer();
    int size = bmp.GetSize();
    static std::vector<glm::vec3> g_vertex_buffer_data;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++)
        {
            vec3 tmp;
            tmp.x = row * 0.1f;
            tmp.y = data[row * width + col] / 255.0f;
            tmp.z = col * 0.1f;
            g_vertex_buffer_data.push_back(tmp);
        }
    }


    // UV坐标：确定顶点颜色在纹理图片上的位置
    /*static GLfloat g_uv_buffer_data[] = {
        0.000059f, 0.000004f,
        0.000103f, 0.336048f,
        0.335973f, 0.335903f,
        1.000023f, 0.000013f,
        0.667979f, 0.335851f,
        0.999958f, 0.336064f,
        0.667979f, 0.335851f,
        0.336024f, 0.671877f,
        0.667969f, 0.671889f,
        1.000023f, 0.000013f,
        0.668104f, 0.000013f,
        0.667979f, 0.335851f,
        0.000059f, 0.000004f,
        0.335973f, 0.335903f,
        0.336098f, 0.000071f,
        0.667979f, 0.335851f,
        0.335973f, 0.335903f,
        0.336024f, 0.671877f,
        1.000004f, 0.671847f,
        0.999958f, 0.336064f,
        0.667979f, 0.335851f,
        0.668104f, 0.000013f,
        0.335973f, 0.335903f,
        0.667979f, 0.335851f,
        0.335973f, 0.335903f,
        0.668104f, 0.000013f,
        0.336098f, 0.000071f,
        0.000103f, 0.336048f,
        0.000004f, 0.671870f,
        0.336024f, 0.671877f,
        0.000103f, 0.336048f,
        0.336024f, 0.671877f,
        0.335973f, 0.335903f,
        0.667969f, 0.671889f,
        1.000004f, 0.671847f,
        0.667979f, 0.335851f
    };*/
    // 纹理图片：BMP
    //GLuint Texture = load_BMP_texture("res/uvtemplate.bmp");
    // 纹理图片：DDS
    // GLuint Texture = loadDDS("res/uvtemplate.DDS");
    /*for (int i = 0; i < 36; i++) {
       float tmp = g_uv_buffer_data[2*i];
       g_uv_buffer_data[2*i] = g_uv_buffer_data[2*i + 1];
       g_uv_buffer_data[2*i + 1] = tmp;
    }*/  // directX反转UV


    // 索引VBO | 以三角形为单位 | width*height个顶点 | (width-1)*(height-1)个矩形 | 2*(width-1)*(height-1)个三角形 | 3*2*(width-1)*(height-1)个序列号 |
    int indexSize = (width - 1) * (height - 1) * 2 * 3;
    static std::vector<GLushort> indices;
    for (int row = 0; row < height-1; row++)
    {
        for (int col = 0; col < width-1; col++)
        {
            // 上三角
            indices.push_back(row * width + col);
            indices.push_back(row * width + col + 1);
            indices.push_back((row + 1) * width + col + 1);
            // 下三角
            indices.push_back(row * width + col);
            indices.push_back((row + 1) * width + col + 1);
            indices.push_back((row + 1) * width + col);
            
        }
    }
    
    /*int indexSize = 6;
    std::vector<GLushort> indices;
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(0);
    indices.push_back(2);
    indices.push_back(3);*/
    
    GLuint elementbuffer;
    glGenBuffers(1, &elementbuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);
    // 顶点buffer
    GLuint vertexbuffer;  // buffer ID
    glGenBuffers(1, &vertexbuffer);  // 创建
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);  // 绑定
    glBufferData(GL_ARRAY_BUFFER, g_vertex_buffer_data.size() * sizeof(glm::vec3), &g_vertex_buffer_data[0], GL_STATIC_DRAW);  // 填充 
    // GL_STATIC_DRAW ：数据不会或几乎不会改变。
    // GL_DYNAMIC_DRAW：数据会被改变很多。
    // GL_STREAM_DRAW ：数据每次绘制时都会改变。
    // 色彩buffer
    /*GLuint colorbuffer;
    glGenBuffers(1, &colorbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
    //glBufferData(GL_ARRAY_BUFFER, sizeof(g_color_buffer_data), g_color_buffer_data, GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, g_color_buffer_data.size() * sizeof(glm::vec3), &g_color_buffer_data[0], GL_STATIC_DRAW);*/
    // 纹理buffer
    /*GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");
    //GLuint uvbuffer;
    //glGenBuffers(1, &uvbuffer);
    //glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    //glBufferData(GL_ARRAY_BUFFER, sizeof(g_uv_buffer_data), g_uv_buffer_data, GL_STATIC_DRAW);
    */
    
    // 变换矩阵
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");
    position = vec3(model_h * 0.5f, 40.0f, model_w * 0.5f);
    vec3 Model_center = glm::vec3(model_h * 0.5f, model_t * 0.5f, model_w * 0.5f);

    double lastTime = glfwGetTime(), FPSTime = glfwGetTime();
    int FPS = 0, gui_FPS = 0;
    
    // 主循环
    do {
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // 清屏+清除深度缓冲区
        glUseProgram(programID);  // 运行着色器
        //glActiveTexture(GL_TEXTURE0);  // 启用纹理单元
        //glBindTexture(GL_TEXTURE_2D, Texture);  // 绑定纹理
        //glUniform1i(TextureID, 0);  // 设置采样器使用纹理单元


        // 显示帧数
        double currentTime = glfwGetTime();
        float deltaTime = float(currentTime - lastTime);
        FPS++;
        if (currentTime - FPSTime >= 1.0)
        {
            printf("FPS: %d\n", FPS);
            gui_FPS = FPS;
            FPS = 0;
            FPSTime += 1.0;
        }

        // 生成变换矩阵
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        glfwSetCursorPos(window, window_width * 0.5, window_height * 0.5);  // 重置光标至窗口中央
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {  // 拉近视野
            scroll_callback(window, 0.0f, (ypos - 384) * 0.05f);
        }
        else {
            horizontalAngle += speed_mouse * float(window_width *0.5 - xpos);
            verticalAngle += speed_mouse * float(window_height *0.5 - ypos);
        }
        

        glm::vec3 direction(
            cos(verticalAngle)* sin(horizontalAngle),
            sin(verticalAngle),
            cos(verticalAngle)* cos(horizontalAngle)
        );  // 视线方向
        glm::vec3 right = glm::vec3(
            sin(horizontalAngle - 3.14f *0.5f),
            0,
            cos(horizontalAngle - 3.14f *0.5f)
        );  // 右向（水平方向）
        glm::vec3 up = glm::cross(right, direction);  // 叉乘获得上方向：与前两者垂直

        if (control_mode)
        {
            float rotation_radius = 0.0f;
            // Closer
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                scroll_callback(window, 0.0f, 0.04f);
            }
            // Farther
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                scroll_callback(window, 0.0f, -0.04f);
            }
            // Rotate
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
                Model = glm::translate(Model, Model_center);
                Model = glm::rotate(Model, glm::radians(speed_rotate * deltaTime), glm::vec3(0.0f, 0.0f, 1.0f));
                Model = glm::translate(Model, -Model_center);
            }
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
                Model = glm::translate(Model, Model_center);
                Model = glm::rotate(Model, glm::radians(-speed_rotate * deltaTime), glm::vec3(0.0f, 0.0f, 1.0f));
                Model = glm::translate(Model, -Model_center);
            }
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
                Model = glm::translate(Model, Model_center);
                Model = glm::rotate(Model, glm::radians(speed_rotate * deltaTime), glm::vec3(0.0f, 1.0f, 0.0f));
                Model = glm::translate(Model, -Model_center);
            }
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                Model = glm::translate(Model, Model_center);
                Model = glm::rotate(Model, glm::radians(-speed_rotate * deltaTime), glm::vec3(0.0f, 1.0f, 0.0f));
                Model = glm::translate(Model, -Model_center);
            }
        }
        else {
            // Move forward
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                position += direction * deltaTime * speed_wasd;
            }
            // Move backward
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                position -= direction * deltaTime * speed_wasd;
            }
            // Strafe right
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                position += right * deltaTime * speed_wasd;
            }
            // Strafe left
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                position -= right * deltaTime * speed_wasd;
            }
        }
        
        if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
            flag_control_mode = 1;
        }
        if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_RELEASE && flag_control_mode)
        {
            flag_control_mode = 0;
            control_mode = control_mode == 1 ? 0 : 1;
        }
        // 点线面绘制切换
        if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
            flag_display_mode = 1;
        } else if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE && flag_display_mode) {
            flag_display_mode = 0;
            switch (display_mode)
            {
            case GL_LINE:
                display_mode = GL_POINT;
                break;
            case GL_POINT:
                display_mode = GL_FILL;
                break;
            case GL_FILL:
                display_mode = GL_LINE;
                break;
            default:
                break;
            }
            glPolygonMode(GL_FRONT_AND_BACK, display_mode); // 设置绘制方式: GL_LINE线框 GL_POINT点 GL_FILL填充
        }
        //Model = translation * rotation * scaling * Model;  // Model矩阵生成遵循 缩放=>旋转=>位移 的顺序，防止相互影响
        Projection = glm::perspective(glm::radians(FoV), window_ratio, 0.1f, 100.0f);  // 透视矩阵：45°视场， 4/3比例， 0.1~100显示范围
        //Projection = glm::ortho(-FoV, FoV, -FoV, FoV, 0.0f, 100.0f);  // 正交透视矩阵, 远近比例不变
        View = glm::lookAt(
            position,  // 相机位置
            position + direction,  // 目标位置
            up   // (0,1,0) or (0,-1,0)
        );  // lookat矩阵
        MVP = Projection * View * Model;  // 合成 Model | View | Projection
        lastTime = currentTime;
        // 传递变换矩阵
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);  // 位置；矩阵个数；是否置换；矩阵数据



        // 1rst attribute buffer : vertices
        glEnableVertexAttribArray(0);  // layout(location)
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glVertexAttribPointer(
            0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
            3,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            (void*)0            // array buffer offset
        );

        // 2nd attribute buffer : colors
        /*//for (int v = 0; v < 12 * 3; v++) {
        //    g_color_buffer_data[3 * v + 0] = abs(g_vertex_buffer_data[3 * v + 0] + g_vertex_buffer_data[3 * v + 1] + g_vertex_buffer_data[3 * v + 2]) / 3.0f * rand() / double(RAND_MAX);
        //    g_color_buffer_data[3 * v + 1] = abs(g_vertex_buffer_data[3 * v + 0] + g_vertex_buffer_data[3 * v + 1] + g_vertex_buffer_data[3 * v + 2]) / 3.0f * rand() / double(RAND_MAX);
        //    g_color_buffer_data[3 * v + 2] = abs(g_vertex_buffer_data[3 * v + 0] + g_vertex_buffer_data[3 * v + 1] + g_vertex_buffer_data[3 * v + 2]) / 3.0f * rand() / double(RAND_MAX);
        //}
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
        //glBufferData(GL_ARRAY_BUFFER, sizeof(g_color_buffer_data), g_color_buffer_data, GL_STATIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, g_color_buffer_data.size() * sizeof(glm::vec3), &g_color_buffer_data[0], GL_STATIC_DRAW);
        glVertexAttribPointer(
            1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
            3,                                // size
            GL_FLOAT,                         // type
            GL_FALSE,                         // normalized?
            0,                                // stride
            (void*)0                          // array buffer offset
        );*/

        // 2nd buffer : UVs
        /*glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
        glVertexAttribPointer(
            1,                  // 匹配着色器layout的location
            2,                  // U & V
            GL_FLOAT,
            GL_FALSE,
            0,
            (void*)0            // 数组偏移量
        );*/

        
        // glDrawArrays:直接绘制顶点，重复传输顶点数据 | glDrawElements：按索引绘制顶点，减少顶点数据传输
        //glDrawArrays(GL_TRIANGLES, 0, 3*12); // 绘制三角形! Starting from vertex 0; 3 vertices total -> 1 triangle
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
        glEnableClientState(GL_VERTEX_ARRAY);
        glDrawElements(
            GL_TRIANGLES,
            indices.size(),
            GL_UNSIGNED_SHORT,
            (void*)0
        );


        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        //ImGui::ShowDemoWindow(&show_demo_window);
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(220.0f, 270.0f));
        ImGui::Begin("GUI");  // GUI标题
        ImGui::SameLine();
        ImGui::Text("FPS: %d", gui_FPS);
        ImGui::Separator();
        ImGui::Text("Help: ");
        ImGui::BulletText("F1: switch display mode");
        ImGui::BulletText("F2: switch control mode");
        ImGui::BulletText("Mouse right press: scaling");
        ImGui::BulletText("Mouse scrolling: scaling");
        ImGui::BulletText("ESC: quit");
        ImGui::Separator();
        if (control_mode)
        {
            ImGui::Text("Control mode 1 (Active): ");
            ImGui::BulletText("Arrow keys: rotation");
            ImGui::BulletText("E & D: scaling");
            ImGui::Separator();
            ImGui::Text("Control mode 2: ");
        }
        else {
            ImGui::Text("Control mode 1: ");
            ImGui::BulletText("Arrow keys: rotation");
            ImGui::BulletText("E & D: scaling");
            ImGui::Separator();
            ImGui::Text("Control mode 2 (Active): ");
        }
        ImGui::BulletText("WASD: move");
        ImGui::Separator();
        ImGui::Text("Made by ZhaoYueyi");
        ImGui::End();
        ImGui::Render();


        glDisableVertexAttribArray(0);
        //glDisableVertexAttribArray(1);
        glDisableClientState(GL_VERTEX_ARRAY);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);  // Swap buffers
        glfwPollEvents();  // 轮询事件

        
    } // Check if the ESC key was pressed or the window was closed
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
        glfwWindowShouldClose(window) == 0);
    // 清理VAO和着色器
    glDeleteBuffers(1, &vertexbuffer);
    //glDeleteBuffers(1, &colorbuffer);
    // glDeleteBuffers(1, &uvbuffer);
    glDeleteBuffers(1, &elementbuffer);
    glDeleteProgram(programID);
    // glDeleteTextures(1, &Texture);
    glDeleteVertexArrays(1, &VertexArrayID);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    

    return 0;
}

