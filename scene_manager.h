#ifndef SCENE_H
#define SCENE_H


#include "shader.h" 
#include "model.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class SceneUtils{

  public:
  SceneUtils()
  {

  }

  //hard coded first test render scene method
  void renderScene_test(Shader &shader, Model &model1, Model &model2, Model &model3) {
      // Object 1
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
      model = glm::scale(model, glm::vec3(0.5f));
      shader.setMat4("model", model);
      model1.Draw(shader);
      
      // Object 2
      model = glm::mat4(1.0f);
      model = glm::translate(model, glm::vec3(3.0f, -1.0f, 1.0f));
      model = glm::scale(model, glm::vec3(0.4f));
      model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
      shader.setMat4("model", model);
      model2.Draw(shader);
      
      // Object 3 
      model = glm::mat4(1.0f);
      model = glm::translate(model, glm::vec3(-3.0f, 1.5f, -1.0f));
      model = glm::scale(model, glm::vec3(0.6f));
      shader.setMat4("model", model);
      model3.Draw(shader);
  }


  void renderScene(Shader &shader, vector<Model*> &models)
  {
      int i {0};
      for (Model* model_it: models ){
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f + i, 0.0f + i, 0.0f));
        model = glm::scale(model, glm::vec3(0.5f));
        shader.setMat4("model", model);
        model_it->Draw(shader);
        ++i;
      }

  }

  void renderModel(Shader &shader, Model* Model, glm::vec3 position, glm::vec3 scale)
  {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::scale(model, scale);
        shader.setMat4("model", model);
        Model->Draw(shader);

  }

  void processShaderPipeline(
      unsigned int &albedo,
      unsigned int &normal,
      unsigned int &metallic,
      unsigned int &roughness,
      unsigned int &ao,
      unsigned int &depthMap,
      Shader &pbrShader,
      Model* model,
      glm::vec3 model_position,
      glm::vec3 model_scale
      )
  {

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, metallic);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, roughness);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, ao);
    glActiveTexture(GL_TEXTURE5);  // Shadow map
    glBindTexture(GL_TEXTURE_2D, depthMap);

    renderModel(pbrShader, model, model_position, model_scale);
    
  }

};

#endif
