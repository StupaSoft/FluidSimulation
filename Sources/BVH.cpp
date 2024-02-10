#include "BVH.h"

void BVH::AddPropObject(const std::shared_ptr<MeshObject> &propObject)
{
    _propObjects.push_back(propObject);
}

bool BVH::GetIntersection(glm::vec3 currentPosition, glm::vec3 nextPosition, Intersection *intersection)
{
    bool isHit = false;
    float minDistance = std::numeric_limits<float>::infinity();
    glm::vec3 ray = nextPosition - currentPosition;

    for (const auto &propObject : _propObjects)
    {
        if (propObject->IsCollidable())
        {
            Intersection propIntersection;

            const auto &triangles = propObject->GetWorldTriangles();
            size_t triangleCount = triangles.size();
            for (size_t i = 0; i < triangleCount; ++i)
            {
                const auto &triangle = triangles[i];
                if (MollerTrumbore(triangle, currentPosition, nextPosition, &propIntersection))
                {
                    float distance = glm::distance(currentPosition, propIntersection.point);
                    if (distance < minDistance)
                    {
                        minDistance = distance;
                        *intersection = std::move(propIntersection);
                        isHit = true;
                    }
                }
            }
        }
    }

    return isHit;
}

bool BVH::MollerTrumbore(const Triangle triangle, glm::vec3 start, glm::vec3 end, Intersection *intersection)
{
    // Moller-Trumbore alogrithm
    constexpr float EPSILON = std::numeric_limits<float>::epsilon();

    glm::vec3 ray = end - start;

    glm::vec3 edge1 = triangle.B - triangle.A;
    glm::vec3 edge2 = triangle.C - triangle.A;
    glm::vec3 rayCrossEdge2 = glm::cross(ray, edge2);
    float det = glm::dot(edge1, rayCrossEdge2);

    if (det > -EPSILON && det < EPSILON) return false; // This ray is parallel to this triangle.

    float inverseDet = 1.0f / det;
    glm::vec3 s = start - triangle.A;
    float u = inverseDet * glm::dot(s, rayCrossEdge2);

    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 sCrossEdge1 = glm::cross(s, edge1);
    float v = inverseDet * glm::dot(ray, sCrossEdge1);

    if (v < 0.0f || u + v > 1.0f) return false;

    // At this stage we can compute t to find out where the intersection point is on the line.
    float t = inverseDet * dot(edge2, sCrossEdge1);
    if (0.0f <= t && t <= 1.0f)
    {
        intersection->point = start + t * ray;
        intersection->normal = glm::normalize((1.0f - u - v) * triangle.normalA + u * triangle.normalB + v * triangle.normalC);
        intersection->pointVelocity = glm::vec3(); // Temp

        return true;
    }
    else
    {
        return false;
    }
}

