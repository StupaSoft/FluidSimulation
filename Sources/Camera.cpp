#include "Camera.h"

void Camera::SetPosition(glm::vec3 position) 
{ 
	_position = position;

	_onChanged.Invoke(*this);
}

void Camera::SetDirection(glm::vec3 direction) 
{ 
	_direction = direction;

	_onChanged.Invoke(*this);
}

void Camera::SetFOV(float fovy)
{ 
	_fovy = fovy;

	_onChanged.Invoke(*this);
};

void Camera::SetExtent(uint32_t width, uint32_t height)
{
	_width = width;
	_height = height;

	_onChanged.Invoke(*this);
}

glm::mat4 Camera::GetViewMatrix() const
{
	// Eye position, target center position, up axis
	glm::mat4 view = glm::lookAt(_position, _direction, glm::vec3(0.0f, 1.0f, 0.0f));
	
	return view;
}

glm::mat4 Camera::GetProjectionMatrix() const
{
	// Vertical field of view, aspect ratio, clipping planes
	glm::mat4 projection = glm::perspective(_fovy, _width / (float)_height, 0.1f, 100.0f);

	// glm was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted.
	// Compensate this inversion.
	projection[1][1] *= -1;

	return projection;
}
