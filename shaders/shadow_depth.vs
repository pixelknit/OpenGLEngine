#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in mat4 aInstanceModel; // consumes locations 5-8

uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform bool instanced;

void main() {
    mat4 modelMatrix = instanced ? aInstanceModel : model;
    gl_Position = lightSpaceMatrix * modelMatrix * vec4(aPos, 1.0);
}
