/*
  This file is part of KDGpu.

  SPDX-FileCopyrightText: 2025 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: MIT

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/
#pragma once

#include "geometric_primitives.h"

#include <KDXr/kdxr_core.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

struct DrawnShape {
    glm::vec3 color;
    glm::vec3 position;
    glm::vec3 scale;
    glm::quat rotation;
    ShapeType shape;
    glm::mat4 transform() const;
};

class DrawingModel
{
public:
    DrawingModel();
    DrawingModel(const DrawingModel &other);
    virtual ~DrawingModel();
    void load(std::string drawingFile);
    void save(std::string drawingFile) const;
    void clear();
    const std::vector<DrawnShape> shapes() const;
    void truncate(size_t count);
    void addShape(const DrawnShape &shape);

private:
    std::vector<DrawnShape> m_drawnShapes;
};
