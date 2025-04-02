#include "DirectionalLight.h"

DirectionalLight::DirectionalLight(glm::vec3 direction, glm::vec3 color, float intensity)
{
	SetDirection(direction);
	SetColor(color);
	SetIntensity(intensity);
}

void DirectionalLight::SetDirection(glm::vec3 direction)
{
	_direction = glm::normalize(direction);

	_onChanged.Invoke(*this);
}

void DirectionalLight::SetColor(glm::vec3 color)
{
	_color = color;

	_onChanged.Invoke(*this);
}

void DirectionalLight::SetIntensity(float intensity)
{
	_intensity = intensity;

	_onChanged.Invoke(*this);
}
