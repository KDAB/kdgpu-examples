/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/
#include "drawing_model.h"

#include <KDUtils/file.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <array>

glm::mat4 DrawnShape::transform() const
{
    glm::mat4 mScale = glm::scale(glm::mat4(1.0f), glm::vec3(scale.x, scale.y, scale.z));
    glm::mat4 mRot = glm::toMat4(rotation);
    glm::mat4 mTrans = glm::translate(glm::mat4(1.0f), position);
    return mTrans * mRot * mScale;
}

DrawingModel::DrawingModel() = default;

DrawingModel::~DrawingModel() = default;

DrawingModel::DrawingModel(const DrawingModel &other) = default;

void DrawingModel::load(std::string drawingFile)
{
    std::ifstream drawingJsonFile(drawingFile);
    json drawingData = json::parse(drawingJsonFile);
    m_drawnShapes.clear();
    for (const auto &shapeData : drawingData) {
        DrawnShape shape;
        // Parse color from hex string
        std::string colorHex = shapeData["color"];
        unsigned int colorInt = std::stoul(colorHex.substr(1), nullptr, 16);
        shape.color = glm::vec3(
                ((colorInt >> 16) & 0xFF) / 255.0f,
                ((colorInt >> 8) & 0xFF) / 255.0f,
                (colorInt & 0xFF) / 255.0f);
        // Parse position
        // The previous version of the app seems to have had units in cm
        shape.position = glm::vec3(
                static_cast<float>(shapeData["position"]["x"]) / 100.0f,
                static_cast<float>(shapeData["position"]["y"]) / 100.0f,
                static_cast<float>(shapeData["position"]["z"]) / 100.0f);
        // Parse rotation
        shape.rotation = glm::quat(
                shapeData["rotation"]["w"],
                shapeData["rotation"]["x"],
                shapeData["rotation"]["y"],
                shapeData["rotation"]["z"]);
        // Parse scale
        shape.scale = glm::vec3(
                shapeData["scale"]["x"],
                shapeData["scale"]["y"],
                shapeData["scale"]["z"]);
        shape.shape = indexShape(shapeData["shape"]);
        m_drawnShapes.push_back(shape);
    }
}
void DrawingModel::save(std::string drawingFile) const
{
    json drawingData = json::array();
    for (const auto &shape : m_drawnShapes) {
        json shapeData;
        // Convert color to hex string
        unsigned int colorInt =
                (static_cast<int>(shape.color.r * 255) << 16) |
                (static_cast<int>(shape.color.g * 255) << 8) |
                static_cast<int>(shape.color.b * 255);
        std::stringstream colorHex;
        colorHex << "#" << std::hex << std::setw(6) << std::setfill('0') << colorInt;
        shapeData["color"] = colorHex.str();
        // Add position
        shapeData["position"] = {
            { "x", shape.position.x * 100.0f },
            { "y", shape.position.y * 100.0f },
            { "z", shape.position.z * 100.0f }
        };
        // Add rotation
        shapeData["rotation"] = {
            { "w", shape.rotation.w },
            { "x", shape.rotation.x },
            { "y", shape.rotation.y },
            { "z", shape.rotation.z }
        };
        // Add scale
        shapeData["scale"] = {
            { "x", shape.scale.x },
            { "y", shape.scale.y },
            { "z", shape.scale.z }
        };
        shapeData["shape"] = shapeIndex(shape.shape);
        drawingData.push_back(shapeData);
    }
    std::ofstream drawingJsonFile(drawingFile);
    drawingJsonFile << drawingData.dump(4); // Pretty print with 4 spaces
}

void DrawingModel::clear()
{
    m_drawnShapes.clear();
}

const std::vector<DrawnShape> DrawingModel::shapes() const
{
    return m_drawnShapes;
}

void DrawingModel::addShape(const DrawnShape &shape)
{
    m_drawnShapes.push_back(shape);
}

void DrawingModel::truncate(size_t count)
{
    if (count < m_drawnShapes.size()) {
        m_drawnShapes.resize(count);
    }
}
