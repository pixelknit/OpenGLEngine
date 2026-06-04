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
    //TexCoords = vec2(aTexCoords.x, 1.0 - aTexCoords.y);
    TexCoords = aTexCoords;
    WorldPos = vec3(model * vec4(aPos, 1.0));
    FragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * aNormal);

    // Gram-Schmidt: re-derive T perpendicular to N, fall back if T is NaN/degenerate
    vec3 Traw = normalMatrix * aTangent;
    float Tlen = dot(Traw, Traw);
    vec3 T;
    if (Tlen > 1e-10 && Tlen == Tlen) {       // Tlen != Tlen catches NaN
        T = normalize(Traw - dot(Traw, N) * N);
    } else {
        vec3 up = abs(N.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        T = normalize(cross(N, up));
    }
    vec3 B = cross(N, T);                      // always orthogonal; ignores aBitangent
    TBN = mat3(T, B, N);
    Normal = N;
    
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
