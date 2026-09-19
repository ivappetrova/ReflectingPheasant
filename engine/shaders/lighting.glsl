// Lighting calculation functions
// Requires: common.glsl

#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

// Distance attenuation for point/spot lights
float calculateDistanceAttenuation(float distance, float range) {
    float normalizedDist = distance / range;
    float attenuation = max(0.0, 1.0 - normalizedDist * normalizedDist);
    return attenuation * attenuation; // Smooth falloff
}

// Spot light cone falloff
float calculateSpotFalloff(vec3 lightToPoint, vec3 spotDirection, float spotAngle, float spotSoftness) {
    float cosAngle = dot(normalize(lightToPoint), spotDirection);
    float cosOuter = cos(spotAngle);
    float cosInner = cos(spotAngle * (1.0 - spotSoftness));
    return smoothstep(cosOuter, cosInner, cosAngle);
}

// Blinn-Phong specular calculation using Geometric Algebra principles
// The half-vector represents the bisector plane between light and view directions
// In GA: the dot product is the symmetric inner product (grade-0 part)
float calculateSpecular(vec3 normal, vec3 lightDir, vec3 viewDir, float shininess) {
    // Half-vector: bisector of light and view directions
    vec3 halfDir = normalize(lightDir + viewDir);
    // Inner product: projects normal onto half-vector direction
    float NdotH = max(dot(normal, halfDir), 0.0);
    return pow(NdotH, shininess);
}

// Calculate diffuse lighting using Lambert's cosine law
// In GA: dot product computes the projection (grade-0 contraction)
// This is equivalent to the cosine of angle between normal and light direction
float calculateDiffuse(vec3 normal, vec3 lightDir) {
    // Inner product: cos(θ) = n·l for unit vectors
    return max(dot(normal, lightDir), 0.0);
}

// Calculate light contribution with all factors
// Returns the combined diffuse + specular contribution
vec3 calculateLightContributionCore(
    Light light,
    vec3 position,
    vec3 normal,
    vec3 viewDir,
    vec3 baseColor,
    float shininess,
    float shadowFactor
) {
    vec3 lightDir;
    float attenuation = 1.0;

    int lightType = int(light.type);
    if (lightType == LIGHT_DIRECTIONAL) {
        lightDir = light.direction.xyz;
    } else if (lightType == LIGHT_POINT) {
        vec3 toLight = light.position.xyz - position;
        float dist = length(toLight);
        lightDir = toLight / dist;
        attenuation = calculateDistanceAttenuation(dist, light.range);
    } else if (lightType == LIGHT_SPOT) {
        vec3 toLight = light.position.xyz - position;
        float dist = length(toLight);
        lightDir = toLight / dist;
        attenuation = calculateDistanceAttenuation(dist, light.range);
        attenuation *= calculateSpotFalloff(-lightDir, light.direction.xyz, light.spotAngle, light.spotSoftness);
    }

    // Early out if attenuation is negligible
    if (attenuation < 0.001) {
        return vec3(0.0);
    }

    // Diffuse
    float NdotL = calculateDiffuse(normal, lightDir);
    vec3 diffuse = light.color * light.intensity * NdotL * baseColor;

    // Specular (Blinn-Phong) - adds highlights for material depth
    float spec = calculateSpecular(normal, lightDir, viewDir, shininess);
    vec3 specular = light.color * light.intensity * spec * 0.3;

    return (diffuse + specular) * attenuation * shadowFactor;
}

// Sky gradient for background
vec3 getSkyColor(vec3 rayDirection) {
    return vec3(0.2, 0.1, 0.8);
}

// Default ambient light
vec3 getAmbientLight() {
    return vec3(0.25, 0.27, 0.30);
}

#endif // LIGHTING_GLSL
