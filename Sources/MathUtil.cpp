#include "MathUtil.h"

uint32_t Log(uint32_t n)
{
	uint32_t logN = 0;
	while (n >>= 1) ++logN;
	return logN;
}

float GetRandomValue(float lowerBound, float upperBound)
{
	static std::random_device randomDevice;
	static std::mt19937 generator(randomDevice());

	std::uniform_real_distribution<> distribution(lowerBound, upperBound);
	return static_cast<float>(distribution(generator));
}
