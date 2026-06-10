#version 330 core
layout(location = 0) out vec3 rsmPosition;
layout(location = 1) out vec3 rsmNormal;
layout(location = 2) out vec3 rsmFlux;

in vec3 WorldPos;
in vec3 WorldNormal;
in vec2 TexCoords;

uniform sampler2D albedoMap;
uniform vec3      lightColor;

void main() {
    vec3 albedo = pow(texture(albedoMap, TexCoords).rgb, vec3(2.2));
    rsmPosition = WorldPos;
    rsmNormal   = normalize(WorldNormal);
    // Flux = energy this surface reflects back into the scene
    rsmFlux     = albedo * lightColor;
}
