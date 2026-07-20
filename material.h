#ifndef MATERIAL_H
#define MATERIAL_H

#include "texture.h"
#include <string>

// A PBR texture set for one model. LoadPBR() loads all five maps from a
// folder by naming convention: <folder>/albedo.png, normal.png, metallic.png,
// roughness.png, ao.png.
struct Material {
    unsigned int albedo = 0;
    unsigned int normal = 0;
    unsigned int metallic = 0;
    unsigned int roughness = 0;
    unsigned int ao = 0;

    static Material LoadPBR(const std::string &folder) {
        Material mat;
        mat.albedo    = LoadTexture2D(folder + "/albedo.png");
        mat.normal    = LoadTexture2D(folder + "/normal.png");
        mat.metallic  = LoadTexture2D(folder + "/metallic.png");
        mat.roughness = LoadTexture2D(folder + "/roughness.png");
        mat.ao        = LoadTexture2D(folder + "/ao.png");
        return mat;
    }
};

#endif
