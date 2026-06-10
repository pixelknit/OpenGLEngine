#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 WorldPos;
out vec3 WorldNormal;
out vec2 TexCoords;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main() {
    vec4 wp    = model * vec4(aPos, 1.0);
    WorldPos   = wp.xyz;
    // Correct normal transform handles non-uniform scale
    WorldNormal = normalize(mat3(transpose(inverse(model))) * aNormal);
    TexCoords  = aTexCoords;
    gl_Position = lightSpaceMatrix * wp;
}
