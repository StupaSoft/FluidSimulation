#include "Kernel.h"

float Kernel::GetValue(float distance) const
{
    if (distance >= _r1)
    {
        return 0.0f;
    }
    else
    {
        float x = 1.0f - distance * distance / _r2;
        return static_cast<float>((315.0f * x * x * x) / (64.0f * M_PI * _r3));
    }
}

float Kernel::FirstDerivative(float distance) const
{
    if (distance >= _r1)
    {
        return 0.0f;
    }
    else
    {
        float x = 1.0f - distance / _r1;
        return static_cast<float>(-45.0f * x * x / (M_PI * _r4));
    }
}

float Kernel::SecondDerivative(float distance) const
{
    if (distance >= _r1)
    {
        return 0.0f;
    }
    else
    {
        float x = 1.0f - distance / _r1;
        return static_cast<float>(90.0f * x / (M_PI * _r5));
    }
}

glm::vec3 Kernel::Gradient(float distance, glm::vec3 directionToCenter) const
{
    return -FirstDerivative(distance) * directionToCenter;
}
