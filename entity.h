#ifndef ENTITY_H
#define ENTITY_H

#include "material.h"
#include "model.h"
#include "transform.h"
#include <vector>

// One placeable object in the scene: geometry + its PBR textures + where it sits.
struct Entity {
    Model *model; // active model; what Scene::Render*Pass actually draws
    Material material;
    Transform transform;

    // Optional LOD chain. Empty for regular (non-LOD) entities.
    // lodModels[0] is the highest-detail mesh, lodModels.back() the lowest.
    // lodDistances[i] is the camera distance at which lodModels[i] switches
    // to lodModels[i + 1], so lodDistances.size() == lodModels.size() - 1.
    std::vector<Model *> lodModels;
    std::vector<float> lodDistances;
};

#endif
