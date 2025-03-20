/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "app-window.h"
#include "slint_generated_public.h"

#include "gl_extensions.h"
#include "renderer.h"

#include <glm/glm.hpp>

#if defined(_WIN32)
#include <windows.h>
#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT 0x9587
#endif
#include <GL/gl.h>
#if !defined(_WIN32)
#include <GL/glext.h>
#endif

#include <memory>

#include <cassert>

class RenderingNotifier
{
public:
    explicit RenderingNotifier(slint::ComponentWeakHandle<AppWindow> ui, const KDGpuRenderer *renderer)
        : m_uiHandle(ui)
        , m_renderer(renderer)
    {
    }

    void operator()(slint::RenderingState state, slint::GraphicsAPI)
    {
        switch (state) {
        case slint::RenderingState::RenderingSetup:
            setup();
            break;
        case slint::RenderingState::BeforeRendering:
            assert(glGetError() == GL_NO_ERROR);
            if (auto ui = m_uiHandle.lock()) {
                const auto width = m_renderer->renderTargetWidth();
                const auto height = m_renderer->renderTargetHeight();
                const auto image =
                        slint::Image::create_from_borrowed_gl_2d_rgba_texture(
                                m_texture, slint::Size<uint32_t>{ width, height });
                (*ui)->set_texture(image);
            }
            break;
        case slint::RenderingState::AfterRendering:
            assert(glGetError() == GL_NO_ERROR);
            break;
        case slint::RenderingState::RenderingTeardown:
            teardown();
            break;
        }
    }

private:
    void setup()
    {
        m_glExt = std::make_unique<GLExtensions>();

        assert(glGetError() == GL_NO_ERROR);

        glGenTextures(1, &m_texture);
        assert(m_texture != 0);

        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Import the render target texture to an OpenGL texture using the EXT_external_objects extension
        // https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects.txt
        // https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects_fd.txt
        // https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects_win32.txt

        m_glExt->glCreateMemoryObjectsEXT(1, &m_memoryObject);
        assert(m_memoryObject != 0);

        const auto width = m_renderer->renderTargetWidth();
        const auto height = m_renderer->renderTargetHeight();
        const auto memoryHandle = m_renderer->renderTargetMemoryHandle();
        const auto size = memoryHandle.allocationSize;
#if defined(_WIN32)
        assert(std::holds_alternative<HANDLE>(memoryHandle.handle));
        const auto handle = std::get<HANDLE>(memoryHandle.handle);
        m_glExt->glImportMemoryWin32HandleEXT(m_memoryObject, size, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, handle);
#else
        assert(std::holds_alternative<int>(memoryHandle.handle));
        const auto fd = std::get<int>(memoryHandle.handle);
        m_glExt->glImportMemoryFdEXT(m_memoryObject, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
#endif
        m_glExt->glTextureStorageMem2DEXT(m_texture, 1, GL_RGBA8, width, height, m_memoryObject, 0);

        glBindTexture(GL_TEXTURE_2D, 0);

        assert(glGetError() == GL_NO_ERROR);
    }

    void teardown()
    {
        m_glExt->glDeleteMemoryObjectsEXT(1, &m_memoryObject);
        glDeleteTextures(1, &m_texture);
    }

    slint::ComponentWeakHandle<AppWindow> m_uiHandle;
    const KDGpuRenderer *m_renderer{ nullptr };
    GLuint m_texture{ 0 };
    GLuint m_memoryObject{ 0 };
    std::unique_ptr<GLExtensions> m_glExt;
};

int main(int argc, char **argv)
{
    KDGpuRenderer renderer(600, 600);
    renderer.initializeScene();

    auto ui = AppWindow::create();
    if (auto error = ui->window().set_rendering_notifier(RenderingNotifier(ui, &renderer))) {
        if (*error == slint::SetRenderingNotifierError::Unsupported) {
            std::cerr << "No GL renderer?\n";
        } else {
            std::cerr << "Unknown error\n";
        }
        exit(EXIT_FAILURE);
    }

    renderer.render();
    ui->on_request_change_angle([&renderer, &ui](int angle) {
        // Redraw the scene
        renderer.setAngle(angle);
        renderer.render();

        // Redraw the window
        ui->window().request_redraw();
    });

    ui->run();
}
