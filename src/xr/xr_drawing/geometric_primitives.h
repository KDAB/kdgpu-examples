/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <type_traits>
#include <ranges>

/**
 * @enum ShapeType
 * @brief Enum to represent different types of geometric shapes.
 */
enum class ShapeType {
    Sphere, ///< Represents a sphere shape.
    Cube, ///< Represents a cube shape.
    Cone, ///< Represents a cone shape.
    Count ///< Represents the number of shape types.
};

/**
 * @brief Converts a ShapeType to an unsigned integer.
 * @param shapeType The ShapeType to convert.
 * @return The unsigned integer representation of the ShapeType.
 */
constexpr uint32_t shapeIndex(ShapeType shapeType)
{
    return static_cast<uint32_t>(shapeType);
}

constexpr ShapeType indexShape(unsigned int index)
{
    return static_cast<ShapeType>(index);
}

/**
 * @struct ShapeVertex
 * @brief Represents a vertex with position and normal vectors.
 *
 * The ShapeVertex struct is used to store the position and normal vectors
 * of a vertex in 3D space. It is used in the generation of geometric shapes.
 */
struct ShapeVertex {
    glm::vec3 position; ///< The position of the vertex.
    glm::vec3 normal; ///< The normal vector of the vertex.
};

/**
 * @struct ShapeData
 * @brief Represents the data of a geometric shape, including vertices and indices.
 *
 * The ShapeData struct is used to store the vertices and indices of a geometric shape.
 * It is used to represent shapes such as cubes and spheres.
 */
struct ShapeData {
    std::vector<ShapeVertex> vertices; ///< The vertices of the shape.
    std::vector<uint32_t> indices; ///< The indices of the shape.
};

/**
 * @brief The default number of subdivisions for generating a curved primitive.
 */
constexpr auto DefaultCurvedDivisions = 16u;

/**
 * @brief Generates the data for a cube.
 * @return The ShapeData containing the vertices and indices of the cube.
 *
 * The generateCube function generates the vertices and indices for a cube
 * with flat face normals.
 */
ShapeData generateCube();

/**
 * @brief Generates the data for a sphere.
 * @param subdivisions The number of subdivisions for the sphere. Default is 16.
 * @return The ShapeData containing the vertices and indices of the sphere.
 *
 * The generateSphere function generates the vertices and indices for a sphere
 * with smooth normals. The number of subdivisions determines the level of detail
 * of the sphere.
 */
ShapeData generateSphere(uint32_t subdivisions = DefaultCurvedDivisions);

/**
 * @brief Generates the data for a cone.
 * @param subdivisions The number of subdivisions for the cone. Default is 16.
 * @return The ShapeData containing the vertices and indices of the cone.
 *
 * The generateCone function generates the vertices and indices for a cone
 * with smooth normals for the conical surface, and flat on its base.
 * The number of subdivisions determines the level of detail of the cone.
 */
ShapeData generateCone(uint32_t subdivisions = DefaultCurvedDivisions);

/**
 * @brief Generates a shape of the given type
 * @param shapeType The type of shape to generate.
 * @param subdivisions The number of subdivisions for the shape. Default is 16.
 * @return The ShapeData containing the vertices and indices of the shape.
 *
 * The generateShape function generates the vertices and indices for a shape
 * of the given type. The number of subdivisions determines the level of detail
 * of the shape.
 */
ShapeData generateShape(ShapeType shapeType, uint32_t subdivisions = DefaultCurvedDivisions);
