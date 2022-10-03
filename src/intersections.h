#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>

#include "sceneStructs.h"
#include "utilities.h"

/**
 * Handy-dandy hash function that provides seeds for random number generation.
 */
__host__ __device__ inline unsigned int utilhash(unsigned int a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

// CHECKITOUT
/**
 * Compute a point at parameter value `t` on ray `r`.
 * Falls slightly short so that it doesn't intersect the object it's hitting.
 */
__host__ __device__ glm::vec3 getPointOnRay(Ray r, float t) {
    return r.origin + (t - .0001f) * glm::normalize(r.direction);
}

/**
 * Multiplies a mat4 and a vec4 and returns a vec3 clipped from the vec4.
 */
__host__ __device__ glm::vec3 multiplyMV(glm::mat4 m, glm::vec4 v) {
    return glm::vec3(m * v);
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed cube. Untransformed,
 * the cube ranges from -0.5 to 0.5 in each axis and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */
__host__ __device__ float boxIntersectionTest(Geom box, Ray r,
        glm::vec3 &intersectionPoint, glm::vec3 &normal, bool &outside) {
    Ray q;
    q.origin    =                multiplyMV(box.inverseTransform, glm::vec4(r.origin   , 1.0f));
    q.direction = glm::normalize(multiplyMV(box.inverseTransform, glm::vec4(r.direction, 0.0f)));

    float tmin = -1e38f;
    float tmax = 1e38f;
    glm::vec3 tmin_n;
    glm::vec3 tmax_n;
    for (int xyz = 0; xyz < 3; ++xyz) {
        float qdxyz = q.direction[xyz];
        /*if (glm::abs(qdxyz) > 0.00001f)*/ {
            float t1 = (-0.5f - q.origin[xyz]) / qdxyz;
            float t2 = (+0.5f - q.origin[xyz]) / qdxyz;
            float ta = glm::min(t1, t2);
            float tb = glm::max(t1, t2);
            glm::vec3 n;
            n[xyz] = t2 < t1 ? +1 : -1;
            if (ta > 0 && ta > tmin) {
                tmin = ta;
                tmin_n = n;
            }
            if (tb < tmax) {
                tmax = tb;
                tmax_n = n;
            }
        }
    }

    if (tmax >= tmin && tmax > 0) {
        outside = true;
        if (tmin <= 0) {
            tmin = tmax;
            tmin_n = tmax_n;
            outside = false;
        }
        intersectionPoint = multiplyMV(box.transform, glm::vec4(getPointOnRay(q, tmin), 1.0f));
        normal = glm::normalize(multiplyMV(box.invTranspose, glm::vec4(tmin_n, 0.0f)));
        return glm::length(r.origin - intersectionPoint);
    }
    return -1;
}

// CHECKITOUT
/**
 * Test intersection between a ray and a transformed sphere. Untransformed,
 * the sphere always has radius 0.5 and is centered at the origin.
 *
 * @param intersectionPoint  Output parameter for point of intersection.
 * @param normal             Output parameter for surface normal.
 * @param outside            Output param for whether the ray came from outside.
 * @return                   Ray parameter `t` value. -1 if no intersection.
 */

template<class T>
__host__ __device__  void barycentricInterpolate(T& p, const T& a,
    const T& b, const T& c, const float u, const float v) {
    p = (1.0f - u - v) * a + u * b + v * c;
}

__host__ __device__ float sphereIntersectionTest(Geom sphere, Ray r,
        glm::vec3 &intersectionPoint, glm::vec3 &normal, bool &outside) {
    float radius = .5;

    glm::vec3 ro = multiplyMV(sphere.inverseTransform, glm::vec4(r.origin, 1.0f));
    glm::vec3 rd = glm::normalize(multiplyMV(sphere.inverseTransform, glm::vec4(r.direction, 0.0f)));

    Ray rt;
    rt.origin = ro;
    rt.direction = rd;

    float vDotDirection = glm::dot(rt.origin, rt.direction);
    float radicand = vDotDirection * vDotDirection - (glm::dot(rt.origin, rt.origin) - powf(radius, 2));
    if (radicand < 0) {
        return -1;
    }

    float squareRoot = sqrt(radicand);
    float firstTerm = -vDotDirection;
    float t1 = firstTerm + squareRoot;
    float t2 = firstTerm - squareRoot;

    float t = 0;
    if (t1 < 0 && t2 < 0) {
        return -1;
    } else if (t1 > 0 && t2 > 0) {
        t = min(t1, t2);
        outside = true;
    } else {
        t = max(t1, t2);
        outside = false;
    }

    glm::vec3 objspaceIntersection = getPointOnRay(rt, t);

    intersectionPoint = multiplyMV(sphere.transform, glm::vec4(objspaceIntersection, 1.f));
    normal = glm::normalize(multiplyMV(sphere.invTranspose, glm::vec4(objspaceIntersection, 0.f)));
    if (!outside) {
        normal = -normal;
    }

    return glm::length(r.origin - intersectionPoint);
}

__host__ __device__ void swapVal(float& v1, float& v2) {
    float tmp = v1;
    v1 = v2;
    v2 = tmp;
}
__host__ __device__ bool hitBoundingBox(glm::vec3& rayOrigin, glm::vec3& rayDirection,const glm::vec3& bbox_min,const glm::vec3& bbox_max)
{
    glm::vec3 inv_dir = 1.0f / rayDirection;

    float tx_min = (bbox_min.x - rayOrigin.x) * inv_dir.x;
    float tx_max = (bbox_max.x - rayOrigin.x) * inv_dir.x;
    if (tx_min > tx_max)
    {
        swapVal(tx_min, tx_max);
    }
    float ty_min = (bbox_min.y - rayOrigin.y) * inv_dir.y;
    float ty_max = (bbox_max.y - rayOrigin.y) * inv_dir.y;
    if (ty_min > ty_max)
    {
        swapVal(ty_min, ty_max);
    }
    if ((tx_min > ty_max) || (ty_min > ty_max))
        return false;

    if (ty_min > tx_min)
    {
        tx_min = ty_min;
    }

    if (ty_max < tx_max)
    {
        tx_max = ty_max;
    }

    float tz_min = (bbox_min.z - rayOrigin.z) * inv_dir.z;
    float tz_max = (bbox_max.z - rayOrigin.z) * inv_dir.z;

    if (tz_min > tz_max)
        swapVal(tz_min, tz_max);

    if ((tx_min > tz_max) || (tz_min > tx_max))
        return false;

    if (tz_min > tx_min)
        tx_min = tz_min;

    if (tz_max < tx_max)
        tx_max = tz_max;

    return true;
}

__host__ __device__ float meshIntersectionTest(Geom& geom,Mesh mesh,PrimitiveData primData,Ray ray,
    glm::vec3& intersectionPoint, glm::vec3& normal,glm::vec2& texCoord,glm::vec4& tangent,int& materialId)
{
    glm::vec3 rayOrigin = multiplyMV(geom.inverseTransform, glm::vec4(ray.origin, 1.0f));
    glm::vec3 rayDirection = glm::normalize(multiplyMV(geom.inverseTransform, glm::vec4(ray.direction, 0.0f)));
    bool hit = false;
    float t = FLT_MAX;
    for (int primId = 0; primId < mesh.prim_count; primId++)
    {
        const Primitive& model = primData.primitives[mesh.prim_offset+primId];
        //Test intersect with bounding box first
        //if not intersect with bounding box, do not move to next step
        if (!hitBoundingBox(rayOrigin,rayDirection,model.boundingBoxMin,model.boundingBoxMax))
        {
            continue;
        }
        //Test intersection with triangles
        for (int i = 0; i < model.count; i += 3)
        {
            int f0 = primData.indices[model.index_Offset+i+0];
            int f1 = primData.indices[model.index_Offset + i + 1];
            int f2 = primData.indices[model.index_Offset + i + 2];

            glm::vec3 v0, v1, v2;
            v0 = primData.vertices[model.vertex_Offset + f0];
            v1 = primData.vertices[model.vertex_Offset + f1];
            v2 = primData.vertices[model.vertex_Offset + f2];

            glm::vec3 baryCentric;
            if (glm::intersectRayTriangle(rayOrigin, rayDirection, v0, v1, v2, baryCentric) && baryCentric.z < t)
            {
                //hit triangle
                hit = true;
                //Face normal
                //Barycentric Interpolate
                if (model.normal_Offset >= 0)
                {
                    glm::vec3 n0, n1, n2;
                    n0 = primData.normals[model.normal_Offset+f0];
                    n1 = primData.normals[model.normal_Offset+f1];
                    n2 = primData.normals[model.normal_Offset+f2];
                    barycentricInterpolate<glm::vec3>(normal, n0, n1, n2, baryCentric.x, baryCentric.y);
                }
                //Interpolate UV
                glm::vec2 uv0, uv1, uv2;
                if (model.uv_Offset >= 0)
                {
                    uv0 = primData.texCoords[model.uv_Offset + f0];
                    uv1 = primData.texCoords[model.uv_Offset + f1];
                    uv2 = primData.texCoords[model.uv_Offset + f2];
                }
                //Tangent 
                glm::vec4 tan0, tan1, tan2;
                if (model.tangent_Offset >= 0)
                {
                    tan0 = primData.tangents[model.tangent_Offset + f0];
                    tan1 = primData.tangents[model.tangent_Offset + f1];
                    tan2 = primData.tangents[model.tangent_Offset + f2];
                }
                else
                {
                    //Calculate tangent vector
                    //Mathematics for 3D Game Programming and Computer Graphics, 3rd ed., Section 7.8 

                    glm::vec3 delta1 = v1 - v0;
                    glm::vec3 delta2 = v2 - v0;

                    glm::vec2 deltaUV1 = uv1 - uv0;
                    glm::vec2 deltaUV2 = uv2 - uv0;

                    float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
                    glm::vec3 sdir = glm::vec3((delta1.x * deltaUV2.y - deltaUV1.y * delta2.x) * r,
                        (delta1.y * deltaUV2.y - deltaUV1.y * delta2.y) * r, (delta1.z * deltaUV2.y - deltaUV1.y * delta2.z) * r);

                    glm::vec3 tdir = glm::vec3((delta2.x * deltaUV1.x - deltaUV2.x * delta1.x) * r,
                        (delta2.y * deltaUV1.x - deltaUV2.x * delta1.y) * r, (delta2.z * deltaUV1.x - deltaUV2.x * delta1.z) * r);

                    glm::vec3 factor1 = glm::normalize(sdir-normal*glm::dot(normal,sdir));
                    float factor2 = glm::dot(glm::cross(normal,sdir),tdir)<0.f?-1.f:1.f;
                }
                t = baryCentric.z;
                materialId = model.mat_id;
            }
        }
    }
    Ray rt;
    rt.origin = rayOrigin;
    rt.direction = rayDirection;
    glm::vec3 objSpaceIntersection = getPointOnRay(rt, t);
    intersectionPoint = multiplyMV(geom.transform, glm::vec4(objSpaceIntersection,1.f));
    normal = glm::normalize(multiplyMV(geom.invTranspose,glm::vec4(normal,1)));
    glm::vec3 tempTangent = glm::normalize(multiplyMV(geom.invTranspose, tangent));
    tangent = glm::vec4(tempTangent, tangent.z);
    return glm::length(ray.origin - intersectionPoint);
}