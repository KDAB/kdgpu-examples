/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "renderer.h"

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <libdrm/drm_fourcc.h>

#include <array>
#include <span>

#include <cassert>

class DmaBufDemo
{
public:
    explicit DmaBufDemo(int windowWidth, int windowHeight, const KDGpuRenderer *vulkanRenderer);
    ~DmaBufDemo();

    void renderLoop();

private:
    void setup(int width, int height, const KDGpuRenderer *vulkanRenderer);
    void teardown();

    GLFWwindow *m_window = nullptr;
    GLuint m_vbo = 0;
    GLuint m_vao = 0;
    GLuint m_program = 0;
    GLuint m_textureUniformLocation = 0;
    GLuint m_texture = 0;
};

DmaBufDemo::DmaBufDemo(int windowWidth, int windowHeight, const KDGpuRenderer *vulkanRenderer)
{
    setup(windowWidth, windowHeight, vulkanRenderer);
}

DmaBufDemo::~DmaBufDemo()
{
    teardown();
}

void DmaBufDemo::setup(int windowWidth, int windowHeight, const KDGpuRenderer *vulkanRenderer)
{
    if (!glfwInit()) {
        SPDLOG_ERROR("Failed to initialize GLFW");
        std::abort();
    }

    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);

    m_window = glfwCreateWindow(windowWidth, windowHeight, "KDGpu DMA-BUF example", nullptr, nullptr);
    if (!m_window) {
        SPDLOG_ERROR("Failed to create GLFW window");
        glfwTerminate();
        std::abort();
    }
    glfwMakeContextCurrent(m_window);

    assert(eglGetError() == EGL_SUCCESS);
    assert(glGetError() == GL_NO_ERROR);

    SPDLOG_INFO("OpenGL renderer: {}", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    SPDLOG_INFO("OpenGL version: {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));

    EGLDisplay display = eglGetCurrentDisplay();
    assert(display != EGL_NO_DISPLAY);

    // We'll render a quad covering the whole window
    struct Vertex {
        glm::vec2 position;
        glm::vec2 texCoord;
    };
    static const std::array<Vertex, 4> verts = {
        Vertex{ { -1, 1 }, { 0, 0 } },
        Vertex{ { -1, -1 }, { 0, 1 } },
        Vertex{ { 1, 1 }, { 1, 0 } },
        Vertex{ { 1, -1 }, { 1, 1 } },
    };

    // Create the VBO
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    const auto vertBytes = std::as_bytes(std::span{ verts });
    glBufferData(GL_ARRAY_BUFFER, vertBytes.size(), vertBytes.data(), GL_STATIC_DRAW);

    // Create the VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, texCoord)));

    // Create the shader program

    const GLchar *vertexShader = R"(#version 300 es
layout(location=0) in vec2 position;
layout(location=1) in vec2 texCoord;
out vec2 vertexTexCoord;
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    vertexTexCoord = texCoord;
})";

    const GLchar *fragShader = R"(#version 300 es
precision highp float;
uniform sampler2D sharedTexture;
in vec2 vertexTexCoord;
out vec4 fragColor;
void main()
{
    fragColor = texture(sharedTexture, vertexTexCoord);
})";

    auto compileShader = [](const GLchar *source, GLuint type) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE) {
            GLint len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            std::string log;
            log.resize(len);
            glGetShaderInfoLog(shader, len, nullptr, log.data());
            SPDLOG_ERROR("Failed to compile shader: {}", log);
            std::abort();
        }
        return shader;
    };

    m_program = glCreateProgram();
    GLuint vs = compileShader(vertexShader, GL_VERTEX_SHADER);
    glAttachShader(m_program, vs);
    GLuint fs = compileShader(fragShader, GL_FRAGMENT_SHADER);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint len = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::string log;
        log.resize(len);
        glGetProgramInfoLog(m_program, len, nullptr, log.data());
        SPDLOG_ERROR("Failed to link shader: {}", log);
        std::abort();
    }

    m_textureUniformLocation = glGetUniformLocation(m_program, "sharedTexture");
    assert(m_textureUniformLocation != -1);

    // Create the texture

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Import the Vulkan render target texture to the OpenGL texture using the EGL_EXT_image_dma_buf_import extension
    // https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_image_dma_buf_import.txt

    auto *glEGLImageTargetTexture2DOES = reinterpret_cast<void (*)(GLenum target, EGLImage image)>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    assert(glEGLImageTargetTexture2DOES);

    const auto drmFormatModifier = vulkanRenderer->renderTargetDrmFormatModifier();
    const auto memoryHandle = vulkanRenderer->renderTargetMemoryHandle();
    assert(std::holds_alternative<int>(memoryHandle.handle));
    const auto fd = std::get<int>(memoryHandle.handle);
    SPDLOG_INFO("DMA-BUF fd: {}, DRM format modifier: {:x}", fd, drmFormatModifier);

    const auto memoryPlaneLayouts = vulkanRenderer->renderTargetMemoryPlaneLayouts();
    assert(memoryPlaneLayouts.size() == 1); // R8G8B8A8_UNORM should have just plane 0
    const auto memoryPlaneLayout = memoryPlaneLayouts.front();

    const std::vector<EGLAttrib> attributeList = {
        EGL_WIDTH, vulkanRenderer->renderTargetWidth(),
        EGL_HEIGHT, vulkanRenderer->renderTargetHeight(),
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_XBGR8888,
        EGL_DMA_BUF_PLANE0_FD_EXT, fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLAttrib>(memoryPlaneLayout.offset),
        EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLAttrib>(memoryPlaneLayout.rowPitch),
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<EGLAttrib>(drmFormatModifier & 0xffffffff),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<EGLAttrib>((drmFormatModifier >> 32) & 0xffffffff),
        EGL_NONE
    };
    EGLImage image = eglCreateImage(display,
                                    EGL_NO_CONTEXT,
                                    EGL_LINUX_DMA_BUF_EXT,
                                    static_cast<EGLClientBuffer>(nullptr),
                                    attributeList.data());
    assert(image != EGL_NO_IMAGE);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    assert(glGetError() == GL_NO_ERROR);
    assert(eglGetError() == EGL_SUCCESS);
}

void DmaBufDemo::renderLoop()
{
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        assert(glGetError() == GL_NO_ERROR);

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(m_program);
        glUniform1i(m_textureUniformLocation, 0);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        assert(glGetError() == GL_NO_ERROR);

        glfwSwapBuffers(m_window);
    }
}

void DmaBufDemo::teardown()
{
    glDeleteTextures(1, &m_texture);
    glDeleteProgram(m_program);
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);

    glfwTerminate();
}

int main(int argc, char **argv)
{
    constexpr auto BufferWidth = 800;
    constexpr auto BufferHeight = 800;

    KDGpuRenderer vulkanRenderer(BufferWidth, BufferHeight);
    vulkanRenderer.initializeScene();

    std::atomic<bool> done = false;
    auto textureUpdater = std::thread([&vulkanRenderer, &done] {
        while (!done) {
            vulkanRenderer.updateScene();
            vulkanRenderer.render();
        }
    });

    DmaBufDemo demo(BufferWidth, BufferHeight, &vulkanRenderer);
    demo.renderLoop();

    done = true;
    textureUpdater.join();
}
