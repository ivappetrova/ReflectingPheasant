// Common types and constants for the raytracer
// GPU structures match FlyFish PGA types for easy CPU-GPU data sharing

#ifndef COMMON_GLSL
#define COMMON_GLSL

// Constants
const float PI = 3.14159265359;
const float EPSILON = 0.0001;
const float SHADOW_BIAS = 0.01;
const float MAX_DIST = 1000000.0;

// Light types
const int LIGHT_DIRECTIONAL = 0;
const int LIGHT_POINT = 1;
const int LIGHT_SPOT = 2;

// Primitive types
const int PRIMITIVE_TRIANGLE = 0;
const int PRIMITIVE_SPHERE = 1;
const int PRIMITIVE_PLANE = 2;

// Shading modes
const int SHADING_FLAT = 0;     // Just color, no lighting
const int SHADING_LAMBERT = 1; // Basic diffuse (Lambertian)
const int SHADING_PHONG = 2;   // Diffuse + specular (Blinn-Phong)
const int SHADING_PBR = 3;     // Full PBR (Cook-Torrance)

// Vertex structure (32 bytes) - matches GPUVertex from C++
// Optimized layout: position.xyz + u packed together, normal.xyz + v packed together
// This saves 16 bytes per vertex (33% memory reduction)
struct Vertex {
    vec4 positionU;     // x, y, z, texture_u
    vec4 normalV;       // nx, ny, nz, texture_v
};

// Triangle structure
struct Triangle {
    uint indices[3];
    uint materialIndex;
};

// BVH Node structure (32 bytes)
struct BVHNode {
    vec3 minBounds;
    int leftFirst;
    vec3 maxBounds;
    int triCount;
};

// Material structure (32 bytes) - matches GPUMaterial from C++
// NOTE: Uses individual floats instead of vec3 to match C++ float[3] layout
// (vec3 in std430 aligns to 16 bytes, but float[3] is only 12 bytes)
struct Material {
    // 16 bytes: diffuse RGB + shininess (packed tightly)
    float diffuse_r, diffuse_g, diffuse_b;
    float shininess;
    // 16 bytes: shading mode + texture index + PBR params
    int shadingMode;
    int diffuseTextureIndex;
    float metalness;      // PBR: 0 = dielectric, 1 = metal
    float roughness;      // PBR: 0 = smooth, 1 = rough
};

// Sphere primitive - PGA TriVector representation (48 bytes)
// Layout matches FlyFish TriVector [e032=x, e013=y, e021=z, e123=radius]
// Must match C++ GPUSphere layout exactly
struct Sphere {
    float center_x, center_y, center_z;  // 12 bytes - e032, e013, e021 components
    float radius;                         // 4 bytes - e123 component (repurposed for radius) (16 total)
    float color_r, color_g, color_b;     // 12 bytes
    float shininess;                      // 4 bytes (32 total)
    float reflectivity;                   // 4 bytes
    int shadingMode;                      // 4 bytes
    float metalness;                      // 4 bytes - PBR
    float roughness;                      // 4 bytes (48 total) - PBR
};

// Plane primitive (48 bytes)
// Layout: normal + distance, color + padding, reflectivity/mode/PBR
// Must match C++ GPUPlane layout exactly
struct Plane {
    float normal_x, normal_y, normal_z;  // 12 bytes - plane normal
    float distance;                       // 4 bytes - signed distance from origin (16 total)
    float color_r, color_g, color_b;     // 12 bytes
    float _padding;                       // 4 bytes (32 total)
    float reflectivity;                   // 4 bytes
    int shadingMode;                      // 4 bytes
    float metalness;                      // 4 bytes - PBR
    float roughness;                      // 4 bytes (48 total) - PBR
};

// Light structure (64 bytes)
// Optimized layout: pack related data into vec4s to eliminate padding waste
// Must match C++ GPULight layout exactly
struct Light {
    // vec4: position.xyz + type (as float)
    float position_x, position_y, position_z;   // 12 bytes
    float type;                                  // 4 bytes (16 total)
    // vec4: direction.xyz + range
    float direction_x, direction_y, direction_z; // 12 bytes
    float range;                                 // 4 bytes (32 total)
    // vec4: color.rgb + intensity
    float color_r, color_g, color_b;            // 12 bytes
    float intensity;                             // 4 bytes (48 total)
    // vec4: spot parameters + padding
    float spotAngle;                             // 4 bytes
    float spotSoftness;                          // 4 bytes
    float _pad0, _pad1;                          // 8 bytes (64 total)
};

// Ray structure
struct Ray {
    vec3 origin;
    vec3 direction;
};

// Mesh instance - transform for instanced rendering
struct MeshInstance {
    mat4 transform;       // Object-to-world transform
    mat4 invTransform;    // World-to-object transform (for ray transformation)
    uint meshId;          // Which mesh this instance uses
    uint visible;         // Visibility flag (0 or 1)
    vec2 _pad;
};

// Texture info - per-texture metadata for multi-texture support (16 bytes)
struct TextureInfo {
    uint offset;            // Offset into texture data buffer (in vec4 pixels)
    uint width;             // Texture width
    uint height;            // Texture height
    uint _pad;              // Padding to 16 bytes
};

// Mesh info - per-mesh offsets for multi-mesh support (32 bytes)
struct MeshInfo {
    uint vertexOffset;      // Offset into vertex buffer
    uint triangleOffset;    // Offset into triangle buffer
    uint bvhNodeOffset;     // Offset into BVH node buffer
    uint bvhTriIdxOffset;   // Offset into BVH triangle index buffer
    uint triangleCount;     // Number of triangles in this mesh
    uint bvhNodeCount;      // Number of BVH nodes in this mesh
    uvec2 _pad;             // Padding to 32 bytes
};

// Hit information
struct HitInfo {
    bool hit;
    float t;
    vec3 position;
    vec3 normal;
    vec2 uv;
    uint triangleIndex;
    uint materialIndex;
    int primitiveType;
    uint instanceIndex;   // Which instance was hit
};

// Vertex accessor functions (optimized 32-byte layout)
vec3 getVertexPosition(Vertex vtx) {
    return vtx.positionU.xyz;
}

vec3 getVertexNormal(Vertex vtx) {
    return vtx.normalV.xyz;
}

vec2 getVertexTexCoord(Vertex vtx) {
    return vec2(vtx.positionU.w, vtx.normalV.w);
}

#endif // COMMON_GLSL
