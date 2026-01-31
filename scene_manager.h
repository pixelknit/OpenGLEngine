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


  void renderScene(Shader &shader, vector<Model> &models)
  {
      int i {0};
      for (Model &model_it: models ){
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f + i, 0.0f + i, 0.0f));
        model = glm::scale(model, glm::vec3(0.5f));
        shader.setMat4("model", model);
        model_it.Draw(shader);
        ++i;
      }

  }

};

#endif
