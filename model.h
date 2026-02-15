#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "stb_image.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

#include "mesh.h"
#include "shader.h"

using namespace std;

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

class Model {
private:
    string m_Name;
public:
    vector<Texture> textures_loaded;
    vector<Mesh>    meshes;
    string directory;
    bool gammaCorrection;

    Model(string const &name, string const &path, bool gamma = false) : m_Name(name), gammaCorrection(gamma) {
        loadModel(path);
    }

    void Draw(Shader &shader) {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

private:
    void loadModel(string const &path) {
        tinyobj::attrib_t attrib;
        vector<tinyobj::shape_t> shapes;
        vector<tinyobj::material_t> materials;
        string warn, err;

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());
        
        if(!warn.empty()) cout << warn << endl;
        if(!err.empty()) cerr << err << endl;
        if(!ret) exit(1);

        directory = path.substr(0, path.find_last_of('/'));

        for (size_t s = 0; s < shapes.size(); s++) {
            vector<Vertex> vertices;
            vector<unsigned int> indices;
            
            size_t index_offset = 0;
            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
                int fv = shapes[s].mesh.num_face_vertices[f];
                
                for (size_t v = 0; v < fv; v++) {
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                    
                    Vertex vertex;
                    
                    vertex.Position = glm::vec3(
                        attrib.vertices[3*idx.vertex_index+0],
                        attrib.vertices[3*idx.vertex_index+1],
                        attrib.vertices[3*idx.vertex_index+2]
                    );
                    
                    if(idx.normal_index >= 0)
                        vertex.Normal = glm::vec3(
                            attrib.normals[3*idx.normal_index+0],
                            attrib.normals[3*idx.normal_index+1],
                            attrib.normals[3*idx.normal_index+2]
                        );
                    
                    if(idx.texcoord_index >= 0)
                        vertex.TexCoords = glm::vec2(
                            attrib.texcoords[2*idx.texcoord_index+0],
                            attrib.texcoords[2*idx.texcoord_index+1]
                        );
                    else
                        vertex.TexCoords = glm::vec2(0, 0);
                    
                    vertices.push_back(vertex);
                }
                index_offset += fv;
            }
            
            for(unsigned int i = 0; i < vertices.size(); i+=3) {
                if(i+2 >= vertices.size()) break;
                
                glm::vec3 v0 = vertices[i].Position;
                glm::vec3 v1 = vertices[i+1].Position;
                glm::vec3 v2 = vertices[i+2].Position;
                
                glm::vec2 uv0 = vertices[i].TexCoords;
                glm::vec2 uv1 = vertices[i+1].TexCoords;
                glm::vec2 uv2 = vertices[i+2].TexCoords;
                
                glm::vec3 deltaPos1 = v1-v0;
                glm::vec3 deltaPos2 = v2-v0;
                
                glm::vec2 deltaUV1 = uv1-uv0;
                glm::vec2 deltaUV2 = uv2-uv0;
                
                float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
                glm::vec3 tangent = (deltaPos1 * deltaUV2.y   - deltaPos2 * deltaUV1.y)*r;
                glm::vec3 bitangent = (deltaPos2 * deltaUV1.x   - deltaPos1 * deltaUV2.x)*r;
                
                vertices[i].Tangent = tangent;
                vertices[i].Bitangent = bitangent;
                vertices[i+1].Tangent = tangent;
                vertices[i+1].Bitangent = bitangent;
                vertices[i+2].Tangent = tangent;
                vertices[i+2].Bitangent = bitangent;
            }
            
            for(unsigned int i = 0; i < vertices.size(); i++) {
                indices.push_back(i);
            }
            
            Mesh mesh(vertices, indices, textures_loaded);
            meshes.push_back(mesh);
        }
    }
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma) {
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
#endif
