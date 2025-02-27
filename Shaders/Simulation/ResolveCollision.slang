import SimulationModule;

static const uint NONE = -1;

struct Intersection
{
	bool isHit;
	float3 point;
	float3 normal;
	float3 pointVelocity;
}

struct Triangle
{
	float4 A;
	float4 B;
	float4 C;

	float4 normalA;
	float4 normalB;
	float4 normalC;
}

struct AABB
{
	Triangle triangle;
	float4 lowerBound;
	float4 upperBound;
}

struct BVHNode
{
	AABB boundingBox;
	uint level;
	uint parent;
	uint child1;
	uint child2;
}

ConstantBuffer<SimulationSetup> simulationSetup;
ConstantBuffer<SimulationParameters> simulationParameters;

RWStructuredBuffer<BVHNode> nodes;
RWStructuredBuffer<float3> positions;
RWStructuredBuffer<float3> velocities;
RWStructuredBuffer<uint> nodeStack;
[vk::push_constant] ConstantBuffer<uint> maxLevel;

RWStructuredBuffer<float3> nextVelocities;
RWStructuredBuffer<float3> nextPositions;

bool RayBoxIntersection(uint nodeIndex, float3 start, float3 end)
{
	float tMin = 0.0f;
	float tMax = INF;

	float3 ray = end - start;
	AABB boundingBox = nodes[nodeIndex].boundingBox;

	float tx1 = (boundingBox.lowerBound.x - start.x) / ray.x;
	float tx2 = (boundingBox.upperBound.x - start.x) / ray.x;
	tMin = min(max(tx1, tMin), max(tx2, tMin));
	tMax = max(min(tx1, tMax), min(tx2, tMax));

	float ty1 = (boundingBox.lowerBound.y - start.y) / ray.y;
	float ty2 = (boundingBox.upperBound.y - start.y) / ray.y;
	tMin = min(max(ty1, tMin), max(ty2, tMin));
	tMax = max(min(ty1, tMax), min(ty2, tMax));

	float tz1 = (boundingBox.lowerBound.z - start.z) / ray.z;
	float tz2 = (boundingBox.upperBound.z - start.z) / ray.z;
	tMin = min(max(tz1, tMin), max(tz2, tMin));
	tMax = max(min(tz1, tMax), min(tz2, tMax));

	return tMin <= tMax;
}

Intersection MollerTrumbore(Triangle triangle, float3 start, float3 end)
{
	Intersection intersection;
	intersection.isHit = false;

	// Moller-Trumbore alogrithm
	float3 ray = end - start;

	float3 edge1 = (triangle.B - triangle.A).xyz;
	float3 edge2 = (triangle.C - triangle.A).xyz;
	float3 rayCrossEdge2 = cross(ray, edge2);
	float det = dot(edge1, rayCrossEdge2);

	if (det > -EPSILON && det < EPSILON)
	{
		intersection.isHit = false;
		return intersection; // This ray is parallel to this triangle.
	} 

	float inverseDet = 1.0f / det;
	float3 s = start - triangle.A.xyz;
	float u = inverseDet * dot(s, rayCrossEdge2);

	if (u < 0.0f || u > 1.0f)
	{
		intersection.isHit = false;
		return intersection; // This ray is parallel to this triangle.
	} 

	float3 sCrossEdge1 = cross(s, edge1);
	float v = inverseDet * dot(ray, sCrossEdge1);

	if (v < 0.0f || u + v > 1.0f)
	{
		intersection.isHit = false;
		return intersection; // This ray is parallel to this triangle.
	}

	// At this stage we can compute t to find out where the intersection point is on the line.
	float t = inverseDet * dot(edge2, sCrossEdge1);
	if (0.0f <= t && t <= 1.0f)
	{
		intersection.isHit = true;
		intersection.point = start + t * ray;
		intersection.normal = normalize(((1.0f - u - v) * triangle.normalA + u * triangle.normalB + v * triangle.normalC).xyz);
		intersection.pointVelocity = float3(0.0f, 0.0f, 0.0f); // Temp

		return intersection;
	}
	else
	{
		intersection.isHit = false;
		return intersection; // This ray is parallel to this triangle.
	}
}

Intersection GetIntersection(uint particleIndex, float3 start, float3 end)
{
	Intersection intersection;
	intersection.isHit = false;

	float minDistance = INF;

	// Traverse the BVH to find a collision point
	uint stackBegin = particleIndex * maxLevel;
	uint count = stackBegin; // # of elements in the stack

	if (RayBoxIntersection(0, start, end))
	{
		nodeStack[count] = 0;
		++count;
	}
	
	while (count != stackBegin)
	{
		--count; // Pop
		uint nodeIndex = nodeStack[count];

		BVHNode node = nodes[nodeIndex];
		if (node.child1 != NONE)
		{
			// This is not a leaf node
			// Push child nodes
			uint child1 = node.child1;
			if (RayBoxIntersection(child1, start, end))
			{
				nodeStack[count] = child1;
				++count;
			}

			uint child2 = node.child2;
			if (RayBoxIntersection(child2, start, end))
			{
				nodeStack[count] = child2;
				++count;
			}
		}
		else
		{
			// This is a leaf node
			Intersection triangleIntersection = MollerTrumbore(node.boundingBox.triangle, start, end);
			if (triangleIntersection.isHit)
			{
				float dist = distance(start, triangleIntersection.point);
				if (dist < minDistance)
				{
					minDistance = dist;
					intersection = triangleIntersection;
				}
			}
		}
	}

	return intersection;
}

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID) 
{
    uint particleIndex = globalThreadID.x;
	if (particleIndex >= simulationSetup.particleCount) return;

	// Resolve collision
	Intersection intersection = GetIntersection(particleIndex, positions[particleIndex], nextPositions[particleIndex]);
	if (intersection.isHit)
	{
		// Target point is the closest non-penetrating position from the current position.
		float3 targetNormal = intersection.normal;
		float3 targetPoint = intersection.point + simulationParameters.particleRadius * targetNormal * 0.5f;
		float3 collisionPointVelocity = intersection.pointVelocity;

		// Get new candidate relative velocities from the target point
		float3 relativeVelocity = nextVelocities[particleIndex] - collisionPointVelocity;
		float normalDotRelativeVelocity = dot(targetNormal, relativeVelocity);
		float3 relativeVelocityN = normalDotRelativeVelocity * targetNormal;
		float3 relativeVelocityT = relativeVelocity - relativeVelocityN;

		// Check if the velocity is facing ooposite direction of the surface normal
		if (normalDotRelativeVelocity < 0.0f)
		{
			// Apply restitution coefficient to the surface normal component of the velocity
			float3 deltaRelativeVelocityN = (-simulationParameters.restitutionCoefficient - 1.0f) * relativeVelocityN;
			relativeVelocityN *= -simulationParameters.restitutionCoefficient;

			// Apply friction to the tangential component of the velocity
			if (length(relativeVelocityT) > 0.0f)
			{
				float frictionScale = max(1.0f - simulationParameters.frictionCoefficient * length(deltaRelativeVelocityN) / length(relativeVelocityT), 0.0f);
				relativeVelocityT *= frictionScale;
			}

			// Apply the velocity
			nextVelocities[particleIndex] = relativeVelocityN + relativeVelocityT + collisionPointVelocity;
		}

		// Apply the position
		nextPositions[particleIndex] = targetPoint;
	}
}