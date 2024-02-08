#include "BVH.h"

void BVH::ResolveCollision(const std::vector<Triangle> &triangles, glm::vec3 currentPosition, glm::vec3 currentVelocity, float particleRadius, float restitutionCoefficient, float frictionCoefficient, glm::vec3 *nextPosition, glm::vec3 *nextVelocity)
{
	// Check if the new position is penetrating the surface
    Intersection intersection{};
	if (GetIntersection(triangles, currentPosition, *nextPosition, &intersection))
	{
        // Target point is the closest non-penetrating position from the current position.
        glm::vec3 targetNormal = intersection.normal;
        glm::vec3 targetPoint = intersection.point + particleRadius * targetNormal; // Temp - is it correct to use along normal? Shouldn't we just use intersection.point?
        glm::vec3 collisionPointVelocity = intersection.pointVelocity;

        // Get new candidate relative velocities from the target point
        glm::vec3 relativeVelocity = *nextVelocity - collisionPointVelocity;
        float normalDotRelativeVelocity = glm::dot(targetNormal, relativeVelocity);
        glm::vec3 relativeVelocityN = normalDotRelativeVelocity * targetNormal;
        glm::vec3 relativeVelocityT = relativeVelocity - relativeVelocityN;

		// Check if the velocity is facing ooposite direction of the surface normal
		if (normalDotRelativeVelocity < 0.0f)
		{
			// Apply restitution coefficient to the surface normal component of the velocity
            glm::vec3 deltaRelativeVelocityN = (-restitutionCoefficient - 1.0f) * relativeVelocityN;
            relativeVelocityN *= -restitutionCoefficient;

			// Apply friction to the tangential component of the velocity
            if (relativeVelocityT.length() > 0.0f)
            {
                float frictionScale = std::max(1.0f - frictionCoefficient * deltaRelativeVelocityN.length() / relativeVelocityT.length(), 0.0f);
                relativeVelocityT *= frictionScale;
            }

			// Apply the velocity
            *nextVelocity = relativeVelocityN + relativeVelocityT + collisionPointVelocity;
		}

		// Apply the position
        *nextPosition = targetPoint;
	}
}

bool BVH::GetIntersection(const std::vector<Triangle> &triangles, glm::vec3 currentPosition, glm::vec3 newPosition, Intersection *intersection)
{
    constexpr float EPSILON = std::numeric_limits<float>::epsilon();
    
    bool isHit = false;
    float tMin = std::numeric_limits<float>::infinity();
    glm::vec3 ray = newPosition - currentPosition;

    size_t triangleCount = triangles.size();
	for (size_t i = 0; i < triangleCount; ++i)
	{
		const auto &triangle = triangles[i];

        // Moller-Trumbore alogrithm
        glm::vec3 edge1 = triangle.B - triangle.A;
        glm::vec3 edge2 = triangle.C - triangle.A;
        glm::vec3 rayCrossEdge2 = glm::cross(ray, edge2);
        float det = glm::dot(edge1, rayCrossEdge2);

        if (det > -EPSILON && det < EPSILON) continue; // This ray is parallel to this triangle.

        float inverseDet = 1.0f / det;
        glm::vec3 s = currentPosition - triangle.A;
        float u = inverseDet * glm::dot(s, rayCrossEdge2);

        if (u < 0.0f || u > 1.0f) continue;

        glm::vec3 sCrossEdge1 = glm::cross(s, edge1);
        float v = inverseDet * glm::dot(ray, sCrossEdge1);

        if (v < 0.0f || u + v > 1.0f) continue;

        // At this stage we can compute t to find out where the intersection point is on the line.
        float t = inverseDet * dot(edge2, sCrossEdge1);

        if (EPSILON < t && t < 1.0f && t < tMin)
        {
            tMin = t;
            isHit = true;

            intersection->point = currentPosition + t * ray;
            intersection->normal = glm::normalize((1.0f - u - v) * triangle.normalA + u * triangle.normalB + v * triangle.normalC);
            intersection->pointVelocity = glm::vec3(); // Temp
        }
	}

    return isHit;
}
