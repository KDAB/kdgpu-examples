/*
  This file is part of KDGpu Examples.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <cstdint>

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLbyte = signed char;
using GLshort = short;
using GLint = int;
using GLsizei = int;
using GLubyte = unsigned char;
using GLushort = unsigned short;
using GLuint = unsigned int;
using GLuint64 = uint64_t;
using GLfloat = float;
using GLclampf = float;
using GLdouble = double;
using GLclampd = double;
using GLvoid = void;

class GLExtensions
{
public:
    GLExtensions();

    void glCreateMemoryObjectsEXT(GLsizei n, GLuint *memoryObjects) const
    {
        m_glCreateMemoryObjectsEXT(n, memoryObjects);
    }

    void glDeleteMemoryObjectsEXT(GLsizei n, GLuint *memoryObjects) const
    {
        m_glDeleteMemoryObjectsEXT(n, memoryObjects);
    }

    void glTextureStorageMem2DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset)
    {
        m_glTextureStorageMem2DEXT(texture, levels, internalFormat, width, height, memory, offset);
    }

    void glImportMemoryFdEXT(GLuint memory, GLuint64 size, GLenum handleType, GLint fd) const
    {
        m_glImportMemoryFdEXT(memory, size, handleType, fd);
    }

    void glImportMemoryWin32HandleEXT(GLuint memory, GLuint64 size, GLenum handleType, void *handle) const
    {
        m_glImportMemoryWin32HandleEXT(memory, size, handleType, handle);
    }

private:
    using glCreateMemoryObjectsEXT_T = void (*)(GLsizei n, GLuint *memoryObjects);
    glCreateMemoryObjectsEXT_T m_glCreateMemoryObjectsEXT{ nullptr };

    using glDeleteMemoryObjectsEXT_T = void (*)(GLsizei n, GLuint *memoryObjects);
    glDeleteMemoryObjectsEXT_T m_glDeleteMemoryObjectsEXT{ nullptr };

    using glTextureStorageMem2DEXT_T = void (*)(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
    glTextureStorageMem2DEXT_T m_glTextureStorageMem2DEXT{ nullptr };

    using glImportMemoryFdEXT_T = void (*)(GLuint memory, GLuint64 size, GLenum handleType, GLint fd);
    glImportMemoryFdEXT_T m_glImportMemoryFdEXT{ nullptr };

    using glImportMemoryWin32HandleEXT_T = void (*)(GLuint memory, GLuint64 size, GLenum handleType, void *handle);
    glImportMemoryWin32HandleEXT_T m_glImportMemoryWin32HandleEXT{ nullptr };
};
