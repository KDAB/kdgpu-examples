/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "gl_extensions.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <GL/glx.h>
#endif

#include <cassert>

namespace {

template<typename T>
T glGetProcAddress(const char *name)
{
#if defined(_WIN32)
    return reinterpret_cast<T>(wglGetProcAddress(name));
#else
    return reinterpret_cast<T>(glXGetProcAddress(reinterpret_cast<const GLubyte *>(name)));
#endif
}

} // namespace

GLExtensions::GLExtensions()
{
    m_glCreateMemoryObjectsEXT = glGetProcAddress<glCreateMemoryObjectsEXT_T>("glCreateMemoryObjectsEXT");
    m_glDeleteMemoryObjectsEXT = glGetProcAddress<glDeleteMemoryObjectsEXT_T>("glDeleteMemoryObjectsEXT");
    m_glTextureStorageMem2DEXT = glGetProcAddress<glTextureStorageMem2DEXT_T>("glTextureStorageMem2DEXT");
    m_glImportMemoryWin32HandleEXT = glGetProcAddress<glImportMemoryWin32HandleEXT_T>("glImportMemoryWin32HandleEXT");
    m_glImportMemoryFdEXT = glGetProcAddress<glImportMemoryFdEXT_T>("glImportMemoryFdEXT");
}
