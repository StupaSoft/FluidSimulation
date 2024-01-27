#include "DirectionalLight.h"

void DirectionalLight::SetDirection(glm::vec3 direction)
{
	_direction = direction;

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
