import SimulationModule;

ConstantBuffer<SimulationSetup> simulationSetup;
ConstantBuffer<GridSetup> gridSetup;
ConstantBuffer<SimulationParameters> simulationParameters;

RWStructuredBuffer<float3> positions;
RWStructuredBuffer<uint> accumulations;
RWStructuredBuffer<uint> hashResults;
RWStructuredBuffer<uint> adjacentBuckets; // [# of particles * 8]

int3 PositionToBucketIndex(float3 position, float gridSpacing)
{
	int3 bucketIndex = int3
	(
		int(floor(position.x / gridSpacing)),
		int(floor(position.y / gridSpacing)),
		int(floor(position.z / gridSpacing))
	);

	return bucketIndex;
}

uint BucketIndexToHashKey(int3 bucketIndex, int3 dimension)
{
	int3 wrappedIndex = bucketIndex;
	wrappedIndex.x = bucketIndex.x % int(dimension.x);
	wrappedIndex.y = bucketIndex.y % int(dimension.y);
	wrappedIndex.z = bucketIndex.z % int(dimension.z);

	if (wrappedIndex.x < 0) wrappedIndex.x += int(dimension.x);
	if (wrappedIndex.y < 0) wrappedIndex.y += int(dimension.y);
	if (wrappedIndex.z < 0) wrappedIndex.z += int(dimension.z);

	return uint((wrappedIndex.z * dimension.y + wrappedIndex.y) * dimension.x + wrappedIndex.x);
}

void RecordAdjacentBuckets(uint particleIndex, int3 bucketIndex, float3 position, uint3 dimension, float gridSpacing)
{
	// Refer to the comments in HashGrid::GetadjacentBuckets
	int3 adjacentBucketIndices[8];
	for (uint i = 0; i < OVERLAPPING_BUCKETS; ++i)
	{
		adjacentBucketIndices[i] = bucketIndex;
	}

	if ((bucketIndex.x + 0.5f) * gridSpacing <= position.x)
	{
		adjacentBucketIndices[4].x += 1;
		adjacentBucketIndices[5].x += 1;
		adjacentBucketIndices[6].x += 1;
		adjacentBucketIndices[7].x += 1;
	}
	else
	{
		adjacentBucketIndices[4].x -= 1;
		adjacentBucketIndices[5].x -= 1;
		adjacentBucketIndices[6].x -= 1;
		adjacentBucketIndices[7].x -= 1;
	}

	if ((bucketIndex.y + 0.5f) * gridSpacing <= position.y)
	{
		adjacentBucketIndices[2].y += 1;
		adjacentBucketIndices[3].y += 1;
		adjacentBucketIndices[6].y += 1;
		adjacentBucketIndices[7].y += 1;
	}
	else
	{
		adjacentBucketIndices[2].y -= 1;
		adjacentBucketIndices[3].y -= 1;
		adjacentBucketIndices[6].y -= 1;
		adjacentBucketIndices[7].y -= 1;
	}

	if ((bucketIndex.z + 0.5f) * gridSpacing <= position.z)
	{
		adjacentBucketIndices[1].z += 1;
		adjacentBucketIndices[3].z += 1;
		adjacentBucketIndices[5].z += 1;
		adjacentBucketIndices[7].z += 1;
	}
	else
	{
		adjacentBucketIndices[1].z -= 1;
		adjacentBucketIndices[3].z -= 1;
		adjacentBucketIndices[5].z -= 1;
		adjacentBucketIndices[7].z -= 1;
	}

	// Fill in the SSBO
	for (uint i = 0; i < OVERLAPPING_BUCKETS; ++i)
	{
		adjacentBuckets[particleIndex * OVERLAPPING_BUCKETS + i] = BucketIndexToHashKey(adjacentBucketIndices[i], dimension);
	}
}

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID) 
{
    uint particleIndex = globalThreadID.x;
	if (particleIndex >= simulationSetup.particleCount) return;

	float3 particlePosition = positions[particleIndex];
	float gridSpacing = 2.0f * simulationParameters.particleRadius * simulationParameters.kernelRadiusFactor;

	// Find the bucket index this particle belongs to
	int3 bucketIndex = PositionToBucketIndex(particlePosition, gridSpacing);
	uint hashKey = BucketIndexToHashKey(bucketIndex, gridSetup.dimension.xyz);

	InterlockedAdd(accumulations[hashKey], 1);
	hashResults[particleIndex] = hashKey;

	// Find and record adjacent buckets
	RecordAdjacentBuckets(particleIndex, bucketIndex, particlePosition, gridSetup.dimension.xyz, gridSpacing);
}