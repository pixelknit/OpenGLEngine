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
