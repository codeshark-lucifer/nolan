#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Transform
{
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 getMatrix() const
    {
        glm::mat4 matrix{1.0f};

        matrix = glm::translate(matrix, position);

        matrix = glm::rotate(matrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        matrix = glm::rotate(matrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        matrix = glm::rotate(matrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

        matrix = glm::scale(matrix, scale);

        return matrix;
    }
};