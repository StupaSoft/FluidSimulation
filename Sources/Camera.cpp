#include "Camera.h"

void Camera::SetFOV(float fovy)
{
	_fovy = fovy;
}

void Camera::SetExtent(uint32_t width, uint32_t height)
{
	_width = width;
	_height = height;
}

glm::mat4 Camera::GetViewMatrix()
{
	// Temp

	// The coordinate system for these vectors is same as that of OpenGL.
	// Y-up right coordinate system
	auto eyePos = glm::vec3(5.0f, 5.0f, 5.0f);
	auto lookTarget = glm::vec3(0.0f, 0.0f, 0.0f);
	auto up = glm::vec3(0.0f, 1.0f, 0.0f);

	// Eye position, target center position, up axis
	glm::mat4 view = glm::lookAt(eyePos, lookTarget, up);
	
	return view;
}

glm::mat4 Camera::GetProjectionMatrix()
{
	// Vertical field of view, aspect ratio, clipping planes
	glm::mat4 projection = glm::perspective(_fovy, _width / (float)_height, 0.1f, 10.0f);

	// glm was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted.
	// Compensate this inversion.
	projection[1][1] *= -1;

	return projection;
}
