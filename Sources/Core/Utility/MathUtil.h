#pragma once

#include <random>
#include <algorithm>

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS // Force glm to use radian as arguments
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Force glm to project z into the range [0.0, 1.0]
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

uint32_t Log(uint32_t n);
float GetRandomValue(float lowerBound, float upperBound);

template <typename T>
void Shuffle(const T &iter1, const T &iter2)
{
	static auto randomDevice = std::random_device{};
	static auto randomEngine = std::default_random_engine(randomDevice());
	std::shuffle(iter1, iter2, randomEngine);
}