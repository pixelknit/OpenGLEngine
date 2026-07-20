#ifndef ENTITY_H
#define ENTITY_H

#include "material.h"
#include "model.h"
#include "transform.h"

// One placeable object in the scene: geometry + its PBR textures + where it sits.
struct Entity {
    Model *model;
    Material material;
    Transform transform;
};

#endif
