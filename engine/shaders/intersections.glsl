// Ray intersection functions - Classic ray tracing formulas
// No PGA - uses conventional vector math

#ifndef INTERSECTIONS_GLSL
#define INTERSECTIONS_GLSL

// Ray-Triangle intersection using Moller-Trumbore algorithm
bool intersectTriangle(Ray ray, vec3 v0, vec3 v1, vec3 v2, inout float t, inout vec3 barycoord) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);

    if (abs(a) < EPSILON)
        return false;

    float f = 1.0 / a;
    vec3 s = ray.origin - v0;
    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0)
        return false;

    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);

    if (v < 0.0 || u + v > 1.0)
        return false;

    float tHit = f * dot(edge2, q);

    if (tHit > EPSILON && tHit < t) {
        t = tHit;
        barycoord = vec3(1.0 - u - v, u, v);
        return true;
    }

    return false;
}

// Ray-Sphere intersection (optimized: ray.direction is pre-normalized, so a=1)
bool intersectSphere(Ray ray, vec3 center, float radius, inout float t) {
    vec3 oc = ray.origin - center;
    // Since ray.direction is normalized, a = dot(d,d) = 1.0
    // Simplified: b = 2*dot(oc, d), c = dot(oc,oc) - r^2
    // Using half-b for efficiency: b' = dot(oc, d)
    float half_b = dot(oc, ray.direction);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = half_b * half_b - c;

    if (discriminant < 0.0)
        return false;

    float sqrtD = sqrt(discriminant);
    float t0 = -half_b - sqrtD;
    float t1 = -half_b + sqrtD;

    float tHit = t0;
    if (tHit < EPSILON)
        tHit = t1;

    if (tHit > EPSILON && tHit < t) {
        t = tHit;
        return true;
    }

    return false;
}

// Ray-Plane intersection
// Plane defined by: dot(normal, point) = distance
bool intersectPlane(Ray ray, vec3 normal, float distance, inout float t) {
    float denom = dot(normal, ray.direction);

    if (abs(denom) < EPSILON)
        return false;  // Ray parallel to plane

    float tHit = (distance - dot(normal, ray.origin)) / denom;

    if (tHit > EPSILON && tHit < t) {
        t = tHit;
        return true;
    }

    return false;
}

// Ray-AABB intersection for BVH traversal
// Uses safe inverse direction computation to handle near-zero components
bool intersectAABB(Ray ray, vec3 minBounds, vec3 maxBounds, float tMax) {
    // Compute safe inverse direction (use large value for near-zero components)
    const float INV_DIR_EPSILON = 1e-8;
    vec3 invDir;
    invDir.x = (abs(ray.direction.x) > INV_DIR_EPSILON) ? (1.0 / ray.direction.x) : sign(ray.direction.x) * 1e8;
    invDir.y = (abs(ray.direction.y) > INV_DIR_EPSILON) ? (1.0 / ray.direction.y) : sign(ray.direction.y) * 1e8;
    invDir.z = (abs(ray.direction.z) > INV_DIR_EPSILON) ? (1.0 / ray.direction.z) : sign(ray.direction.z) * 1e8;

    vec3 t0 = (minBounds - ray.origin) * invDir;
    vec3 t1 = (maxBounds - ray.origin) * invDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);

    float enter = max(max(tmin.x, tmin.y), tmin.z);
    float exit = min(min(tmax.x, tmax.y), tmax.z);

    return enter <= exit && exit >= 0.0 && enter < tMax;
}

// Reflection direction
vec3 reflectDirection(vec3 incident, vec3 normal) {
    return incident - 2.0 * dot(incident, normal) * normal;
}

// Compute sphere UV coordinates
vec2 computeSphereUV(vec3 hitPos, vec3 center) {
    vec3 localPos = normalize(hitPos - center);
    float u = 0.5 + atan(localPos.z, localPos.x) / (2.0 * PI);
    float v = 0.5 - asin(clamp(localPos.y, -1.0, 1.0)) / PI;
    return vec2(u, v);
}

// Compute plane UV coordinates (checkerboard-friendly)
vec2 computePlaneUV(vec3 hitPos, vec3 normal, float tileScale) {
    // Build tangent frame
    vec3 tangent = abs(normal.y) < 0.999
        ? normalize(cross(vec3(0.0, 1.0, 0.0), normal))
        : normalize(cross(vec3(1.0, 0.0, 0.0), normal));
    vec3 bitangent = cross(normal, tangent);

    float u = dot(hitPos, tangent) * tileScale;
    float v = dot(hitPos, bitangent) * tileScale;
    return vec2(u, v);
}

// Checkerboard pattern
float checkerboard(vec2 uv) {
    return mod(floor(uv.x) + floor(uv.y), 2.0);
}

#endif // INTERSECTIONS_GLSL
