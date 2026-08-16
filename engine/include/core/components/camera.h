#pragma once

#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "./transform.h"

struct Camera
{
    // Perspective settings
    float fov{60.0f};
    float near_plane{0.1f};
    float far_plane{1000.0f};

    // Render resolution
    uint32_t width{1280};
    uint32_t height{720};

    // ------------------------------------------------------------
    // Projection matrix
    // ------------------------------------------------------------

    glm::mat4 GetProjection() const
    {
        float aspect =
            static_cast<float>(width) /
            static_cast<float>(height);

        return glm::perspective(
            glm::radians(fov),
            aspect,
            near_plane,
            far_plane
        );
    }

    // ------------------------------------------------------------
    // View matrix
    // ------------------------------------------------------------

    glm::mat4 GetView(const Transform& transform) const
    {
        glm::mat4 view{1.0f};

        // Camera position
        view = glm::translate(
            view,
            -transform.position
        );

        // Reverse camera rotation
        view = glm::rotate(
            view,
            -transform.rotation.z,
            glm::vec3(0.0f, 0.0f, 1.0f)
        );

        view = glm::rotate(
            view,
            -transform.rotation.y,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        view = glm::rotate(
            view,
            -transform.rotation.x,
            glm::vec3(1.0f, 0.0f, 0.0f)
        );

        return view;
    }

    // ------------------------------------------------------------
    // View-projection matrix
    // ------------------------------------------------------------

    glm::mat4 GetViewProjection(
        const Transform& transform
    ) const
    {
        return GetProjection() * GetView(transform);
    }

    // ------------------------------------------------------------
    // Resize
    // ------------------------------------------------------------

    void Resize(uint32_t new_width, uint32_t new_height)
    {
        width = new_width;
        height = new_height;
    }
};