#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec2 TexCoords;
out vec3 WorldPos;
out vec3 Normal;
out mat3 TBN;
out vec4 FragPosLightSpace;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix; 

void main() {
    TexCoords = vec2(aTexCoords.x, 1.0 - aTexCoords.y);
    WorldPos = vec3(model * vec4(aPos, 1.0));
    FragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 B = normalize(normalMatrix * aBitangent);
    vec3 N = normalize(normalMatrix * aNormal);
    TBN = mat3(T, B, N);
    Normal = N;
    
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
