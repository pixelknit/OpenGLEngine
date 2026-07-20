#ifndef SCENE_H
#define SCENE_H

#include "entity.h"
#include "material.h"
#include "model.h"
#include "shader.h"
#include "transform.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include <vector>

// Owns every model + entity in the scene and knows how to render them.
// Add an object with one call:
//
//   scene.AddEntity("table", "models/table/table.obj", "models/table",
//                    Transform{ {1.0f, 1.0f, 0.0f}, {}, glm::vec3(0.02f) });
//
// and it participates in both the shadow/RSM depth pass and the main PBR
// pass automatically.
class Scene {
public:
    std::vector<Entity> entities;

    Entity &AddEntity(const std::string &name, const std::string &objPath,
                       const std::string &textureFolder, const Transform &transform) {
        models.push_back(std::make_unique<Model>(name, objPath));
        Entity entity;
        entity.model = models.back().get();
        entity.material = Material::LoadPBR(textureFolder);
        entity.transform = transform;
        entities.push_back(entity);
        return entities.back();
    }

    // Same as AddEntity, but loads a chain of progressively-decimated OBJs
    // and switches between them based on distance to the camera. objPaths[0]
    // must be the highest-detail mesh; each subsequent entry is a lower-detail
    // LOD. lodDistances[i] is the distance at which objPaths[i] hands off to
    // objPaths[i + 1], so lodDistances must have exactly objPaths.size() - 1
    // entries, in ascending order. All LODs share one PBR material.
    //
    //   scene.AddEntityLOD("trench_rock",
    //       {"models/ground_trench_rock01/trench_rock01.obj",
    //        "models/ground_trench_rock01/trench_rock01_LOD1.obj",
    //        "models/ground_trench_rock01/trench_rock01_LOD2.obj"},
    //       {20.0f, 45.0f},
    //       "models/ground_trench_rock01",
    //       Transform{ {1.5f, 0.0f, 0.0f}, {}, glm::vec3(10.0f) });
    Entity &AddEntityLOD(const std::string &name, const std::vector<std::string> &objPaths,
                          const std::vector<float> &lodDistances, const std::string &textureFolder,
                          const Transform &transform) {
        Entity entity;
        for (size_t i = 0; i < objPaths.size(); ++i) {
            models.push_back(std::make_unique<Model>(name + "_LOD" + std::to_string(i), objPaths[i]));
            entity.lodModels.push_back(models.back().get());
        }
        entity.lodDistances = lodDistances;
        entity.model = entity.lodModels.front();
        entity.material = Material::LoadPBR(textureFolder);
        entity.transform = transform;
        entities.push_back(entity);
        return entities.back();
    }

    // Call once per frame (before the render passes) to pick each LOD
    // entity's active model based on distance from the camera. Entities
    // added via plain AddEntity have no lodModels and are skipped.
    void UpdateLOD(const glm::vec3 &camPos) {
        for (Entity &entity : entities) {
            if (entity.lodModels.size() < 2) continue;

            float dist = glm::length(camPos - entity.transform.position);
            size_t level = 0;
            while (level < entity.lodDistances.size() && dist >= entity.lodDistances[level]) {
                ++level;
            }
            entity.model = entity.lodModels[level];
        }
    }

    // Depth-only pass (shadow map / RSM). Only the albedo map is bound since
    // that's all rsmDepthShader samples.
    void RenderShadowPass(Shader &shader) {
        for (Entity &entity : entities) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, entity.material.albedo);
            shader.setMat4("model", entity.transform.GetMatrix());
            entity.model->Draw(shader);
        }
    }

    // Full PBR pass: binds all five material maps plus the shared shadow map
    // and environment map, then draws.
    void RenderPBRPass(Shader &shader, unsigned int envMap, unsigned int depthMap) {
        for (Entity &entity : entities) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, entity.material.albedo);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, entity.material.normal);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, entity.material.metallic);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, entity.material.roughness);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, entity.material.ao);
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, depthMap);
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, envMap);

            shader.setMat4("model", entity.transform.GetMatrix());
            entity.model->Draw(shader);
        }
    }

private:
    std::vector<std::unique_ptr<Model>> models;
};

#endif
