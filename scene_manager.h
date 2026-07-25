#ifndef SCENE_H
#define SCENE_H

#include "entity.h"
#include "material.h"
#include "model.h"
#include "shader.h"
#include "transform.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <memory>
#include <random>
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
        entity.name = name;
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
        entity.name = name;
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

    // Scatters `count` copies of a LOD model chain randomly within `radius`
    // of `center` (uniform over the disc, so density doesn't bunch up at the
    // middle), with a random Y rotation and a random uniform scale drawn from
    // [scaleRange.x, scaleRange.y]. All instances share one Entity and are
    // drawn with a single instanced draw call per submesh per LOD level, so
    // this is far cheaper than `count` separate AddEntityLOD calls. LOD level
    // is chosen once per frame from the camera's distance to `center`, so the
    // whole cluster switches detail together.
    //
    //   scene.AddScatteredEntityLOD("trench_rock",
    //       {"models/ground_trench_rock01/trench_rock01.obj",
    //        "models/ground_trench_rock01/trench_rock01_LOD1.obj",
    //        "models/ground_trench_rock01/trench_rock01_LOD2.obj"},
    //       {20.0f, 45.0f},
    //       "models/ground_trench_rock01",
    //       /*count*/ 40, /*center*/ glm::vec3(1.5f, 0.0f, 0.0f), /*radius*/ 12.0f,
    //       /*scaleRange*/ glm::vec2(0.6f, 1.4f));
    Entity &AddScatteredEntityLOD(const std::string &name, const std::vector<std::string> &objPaths,
                                   const std::vector<float> &lodDistances, const std::string &textureFolder,
                                   unsigned int count, const glm::vec3 &center, float radius,
                                   const glm::vec2 &scaleRange, unsigned int seed = 1337) {
        Entity entity;
        entity.name = name;
        for (size_t i = 0; i < objPaths.size(); ++i) {
            models.push_back(std::make_unique<Model>(name + "_LOD" + std::to_string(i), objPaths[i]));
            entity.lodModels.push_back(models.back().get());
        }
        entity.lodDistances = lodDistances;
        entity.model = entity.lodModels.front();
        entity.material = Material::LoadPBR(textureFolder);

        entity.scatter.enabled = true;
        entity.scatter.count = count;
        entity.scatter.center = center;
        entity.scatter.radius = radius;
        entity.scatter.scaleRange = scaleRange;
        entity.scatter.seed = seed;
        Rescatter(entity);

        entities.push_back(entity);
        return entities.back();
    }

    // Rebuilds a scattered entity's instance transforms from its current
    // entity.scatter values and re-uploads them to every LOD mesh. Call after
    // editing any scatter parameter (the editor UI does this on each change);
    // it costs one buffer upload per submesh per LOD, no mesh reloading.
    void Rescatter(Entity &entity) {
        const ScatterParams &p = entity.scatter;
        if (!p.enabled) return;

        // UpdateLOD() picks the cluster's detail level off transform.position,
        // so anchor it at the scatter center.
        entity.transform.position = p.center;

        std::mt19937 rng(p.seed);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
        std::uniform_real_distribution<float> scaleDist(p.scaleRange.x, p.scaleRange.y);

        entity.instanceTransforms.clear();
        entity.instanceTransforms.reserve(p.count);
        for (unsigned int i = 0; i < p.count; ++i) {
            // sqrt(unit) keeps points uniformly dense across the disc instead
            // of clustering near the center.
            float r = p.radius * std::sqrt(unit(rng));
            float angle = angleDist(rng);
            glm::vec3 position = p.center + glm::vec3(r * std::cos(angle), 0.0f, r * std::sin(angle));

            Transform t;
            t.position = position;
            t.rotation = glm::vec3(0.0f, unit(rng) * p.yRotationJitter, 0.0f);
            t.scale = glm::vec3(scaleDist(rng));
            entity.instanceTransforms.push_back(t.GetMatrix());
        }

        for (Model *lodModel : entity.lodModels)
            lodModel->SetupInstancing(entity.instanceTransforms);
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

            if (entity.IsInstanced()) {
                shader.setBool("instanced", true);
                entity.model->DrawInstanced(shader);
            } else {
                shader.setBool("instanced", false);
                shader.setMat4("model", entity.transform.GetMatrix());
                entity.model->Draw(shader);
            }
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

            if (entity.IsInstanced()) {
                shader.setBool("instanced", true);
                entity.model->DrawInstanced(shader);
            } else {
                shader.setBool("instanced", false);
                shader.setMat4("model", entity.transform.GetMatrix());
                entity.model->Draw(shader);
            }
        }
    }

private:
    std::vector<std::unique_ptr<Model>> models;
};

#endif
