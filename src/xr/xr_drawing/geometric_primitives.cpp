/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "geometric_primitives.h"

namespace {
constexpr auto PI = 3.14159265359f;
constexpr auto TAU = 2.0f * PI;
} // namespace

ShapeData generateCube()
{
    ShapeData shapeData;

    shapeData.vertices = {
        // Front face
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f) },
        { glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f) },
        { glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f) },
        { glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f) },
        // Back face
        { glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f) },
        { glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f) },
        { glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f) },
        { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f) },
        // Left face
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f) },
        { glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f) },
        { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(-1.0f, 0.0f, 0.0f) },
        { glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(-1.0f, 0.0f, 0.0f) },
        // Right face
        { glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f) },
        { glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f) },
        { glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f) },
        { glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f) },
        // Top face
        { glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f) },
        { glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f) },
        { glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f) },
        { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f) },
        // Bottom face
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f) },
        { glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f) },
        { glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f) },
        { glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f) }
    };

    shapeData.indices = {
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        8, 9, 10, 10, 11, 8,
        // Right face
        12, 13, 14, 14, 15, 12,
        // Top face
        16, 17, 18, 18, 19, 16,
        // Bottom face
        20, 21, 22, 22, 23, 20
    };

    return shapeData;
}

ShapeData generateSphere(uint32_t subdivisions)
{
    ShapeData shapeData;

    for (uint32_t y = 0; y <= subdivisions; ++y) {
        for (uint32_t x = 0; x <= subdivisions; ++x) {
            auto u = static_cast<float>(x) / subdivisions;
            auto v = static_cast<float>(y) / subdivisions;

            auto theta = u * TAU;
            auto phi = v * PI;

            auto sinPhi = sin(phi);
            auto cosPhi = cos(phi);
            auto sinTheta = sin(theta);
            auto cosTheta = cos(theta);

            glm::vec3 position = glm::vec3(cosTheta * sinPhi * 0.5, cosPhi * 0.5, sinTheta * sinPhi * 0.5);
            glm::vec3 normal = glm::normalize(position);

            shapeData.vertices.push_back({ position, normal });
        }
    }

    for (auto y = 0u; y < subdivisions; ++y) {
        for (auto x = 0u; x < subdivisions; ++x) {
            auto i0 = y * (subdivisions + 1) + x;
            auto i1 = i0 + 1;
            auto i2 = i0 + (subdivisions + 1);
            auto i3 = i2 + 1;

            shapeData.indices.push_back(i0);
            shapeData.indices.push_back(i2);
            shapeData.indices.push_back(i1);

            shapeData.indices.push_back(i1);
            shapeData.indices.push_back(i2);
            shapeData.indices.push_back(i3);
        }
    }

    return shapeData;
}

ShapeData generateCone(uint32_t subdivisions)
{
    ShapeData shapeData;

    // Generate vertices for the tip of the cone with correct shading
    for (uint32_t i = 0; i < subdivisions; ++i) {
        auto angle = static_cast<float>(i) / subdivisions * TAU;
        auto x = 0.5f * cos(angle);
        auto y = 0.5f * sin(angle);
        glm::vec3 position = glm::vec3(x, y, 0.5f);
        glm::vec3 normal = glm::normalize(glm::vec3(x, y, -0.5f));

        // Vertex for the conical surface
        shapeData.vertices.push_back({ glm::vec3(0.0f, 0.0f, -0.5f), normal });
        shapeData.vertices.push_back({ position, normal });
    }

    const auto coneVertCount = 2 * subdivisions;

    // Base center of the cone
    shapeData.vertices.push_back({ glm::vec3(0.0f, 0.0f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f) });

    // Generate vertices for the base with flat shading
    for (uint32_t i = 0; i < subdivisions; ++i) {
        auto angle = static_cast<float>(i) / subdivisions * TAU;
        auto x = 0.5f * cos(angle);
        auto y = 0.5f * sin(angle);
        glm::vec3 position = glm::vec3(x, y, 0.5f);

        // Vertex for the base with flat shading
        shapeData.vertices.push_back({ position, glm::vec3(0.0f, 0.0f, 1.0f) });
    }

    // Generate indices for the conical surface
    for (uint32_t i = 0; i < 2 * subdivisions; i += 2) {
        shapeData.indices.push_back(i);
        shapeData.indices.push_back(i + 1);
        shapeData.indices.push_back((i + 3) % (coneVertCount));
    }

    // Generate indices for the base
    for (uint32_t i = 0; i < subdivisions; ++i) {
        shapeData.indices.push_back(coneVertCount); // Center of the base
        shapeData.indices.push_back(coneVertCount + 1 + ((i + 1) % subdivisions));
        shapeData.indices.push_back(coneVertCount + 1 + i);
    }

    return shapeData;
}

ShapeData generateShape(ShapeType shapeType, uint32_t subdivisions)
{
    switch (shapeType) {
    case ShapeType::Cube:
        return generateCube();
    case ShapeType::Sphere:
        return generateSphere(subdivisions);
    case ShapeType::Cone:
        return generateCone(subdivisions);
    default:
        return {};
    }
}
