#ifndef ENTITY_H
#define ENTITY_H

#include "material.h"
#include "model.h"
#include "transform.h"
#include <string>
#include <vector>

// How a scattered cluster lays its instances out. Set by
// Scene::AddScatteredEntityLOD and re-readable/editable afterwards: change any
// field and call Scene::Rescatter(entity) to rebuild the instance transforms
// and re-upload them to the GPU. `enabled` is false on non-scattered entities.
struct ScatterParams {
    bool enabled = false;
    unsigned int count = 0;
    glm::vec3 center{0.0f};
    float radius = 1.0f;
    glm::vec2 scaleRange{1.0f, 1.0f};
    float yRotationJitter = 360.0f; // degrees of random yaw applied per instance
    unsigned int seed = 1337;
};

// One placeable object in the scene: geometry + its PBR textures + where it sits.
struct Entity {
    std::string name; // display name, used by the editor UI
    Model *model; // active model; what Scene::Render*Pass actually draws
    Material material;
    Transform transform;

    // Optional LOD chain. Empty for regular (non-LOD) entities.
    // lodModels[0] is the highest-detail mesh, lodModels.back() the lowest.
    // lodDistances[i] is the camera distance at which lodModels[i] switches
    // to lodModels[i + 1], so lodDistances.size() == lodModels.size() - 1.
    std::vector<Model *> lodModels;
    std::vector<float> lodDistances;

    // Per-instance model matrices for GPU-instanced rendering (e.g. a
    // scattered cluster of rocks drawn in one call). Empty for regular
    // entities, which use transform.GetMatrix() instead.
    std::vector<glm::mat4> instanceTransforms;

    // Layout rule that generated instanceTransforms. Only meaningful when
    // scatter.enabled; see Scene::Rescatter.
    ScatterParams scatter;

    bool IsInstanced() const { return !instanceTransforms.empty(); }
};

#endif
