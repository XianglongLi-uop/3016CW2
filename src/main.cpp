#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <irrKlang/irrKlang.h>
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>

using namespace irrklang;
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

glm::vec3 cameraPos = glm::vec3(69.313f, 6.42059f, 9.62967f);
glm::vec3 cameraFront = glm::vec3(-0.119442f, 0.10453f, -0.987323f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw = 983.102f;
float pitch = 6.00011f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
glm::vec3 playerPos = glm::vec3(69.5f, 35.0f, 10.0f);
glm::vec3 playerVelocity = glm::vec3(0.0f);
float playerSpeed = 14.0f;
float sprintMultiplier = 2.0f;
float jumpForce = 18.0f;
float gravity = -25.0f;
bool isGrounded = false;
bool isSprinting = false;
bool flyMode = false;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
ISoundEngine* soundEngine = nullptr;
int playerScore = 0;
ISound* collectSound = nullptr;
float collectSoundTimer = 0.0f;
int activeDebugModel = 0;
std::vector<glm::vec3> forestPositions;
glm::vec3 forestDebugRot = glm::vec3(-94.3518f, 0.0f, 19.3014f);
float forestDebugScale = 0.043134f;
std::vector<glm::vec3> winterPositions;
glm::vec3 winterDebugRot = glm::vec3(-87.9467f, -5.10635f, -3.77178f);
float winterDebugScale = 1.0f;
bool modelDebugMode = false;
glm::vec3 modelDebugPos = glm::vec3(0.0f, 50.0f, 0.0f);
glm::vec3 modelDebugRot = glm::vec3(97.9302f, 5.74408f, 82.0108f);
float modelDebugScale = 15.0f;
glm::vec3 modelBaseRot = glm::vec3(97.9302f, 5.74408f, 82.0108f);

enum PlatformType {
    PLATFORM_BOX = 0,
    PLATFORM_HEMISPHERE = 1,
    PLATFORM_ROTATING = 2,
    PLATFORM_CYLINDER = 3,
    PLATFORM_OBJ = 4,
    PLATFORM_MOVING = 5
};

class PerlinNoise {
private:
    int p[512];

    float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    float lerp(float t, float a, float b) { return a + t * (b - a); }
    float grad(int hash, float x, float y, float z) {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

public:
    PerlinNoise(unsigned int seed = 237) {
        int permutation[256];
        for (int i = 0; i < 256; i++) permutation[i] = i;

        srand(seed);
        for (int i = 255; i > 0; i--) {
            int j = rand() % (i + 1);
            std::swap(permutation[i], permutation[j]);
        }

        for (int i = 0; i < 512; i++) p[i] = permutation[i % 256];
    }

    float noise(float x, float y, float z) {
        int X = (int)floor(x) & 255;
        int Y = (int)floor(y) & 255;
        int Z = (int)floor(z) & 255;

        x -= floor(x);
        y -= floor(y);
        z -= floor(z);

        float u = fade(x), v = fade(y), w = fade(z);

        int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
        int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

        return lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z),
            grad(p[BA], x - 1, y, z)),
            lerp(u, grad(p[AB], x, y - 1, z),
                grad(p[BB], x - 1, y - 1, z))),
            lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1),
                grad(p[BA + 1], x - 1, y, z - 1)),
                lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
                    grad(p[BB + 1], x - 1, y - 1, z - 1))));
    }

    float fbm(float x, float y, int octaves = 6, float persistence = 0.5f) {
        float total = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;
        float maxValue = 0.0f;

        for (int i = 0; i < octaves; i++) {
            total += noise(x * frequency, y * frequency, 0.0f) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= 2.0f;
        }

        return total / maxValue;
    }
};

PerlinNoise perlin(42);

class Shader {
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode, fragmentCode;
        std::ifstream vShaderFile, fShaderFile;

        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);

        std::stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vShaderFile.close();
        fShaderFile.close();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        unsigned int vertex, fragment;
        int success;
        char infoLog[512];

        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertex, 512, NULL, infoLog);
            std::cout << "Vertex shader error: " << infoLog << std::endl;
        }

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment, 512, NULL, infoLog);
            std::cout << "Fragment shader error: " << infoLog << std::endl;
        }

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(ID, 512, NULL, infoLog);
            std::cout << "Shader program link error: " << infoLog << std::endl;
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void use() { glUseProgram(ID); }

    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
};

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    unsigned int VAO;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) {
        this->vertices = vertices;
        this->indices = indices;
        this->textures = textures;
        setupMesh();
    }

    void Draw(Shader& shader) {
        unsigned int diffuseNr = 1;
        for (unsigned int i = 0; i < textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            std::string name = textures[i].type;
            std::string number = std::to_string(diffuseNr++);
            shader.setInt(("texture_" + name + number).c_str(), i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

private:
    unsigned int VBO, EBO;

    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

        glBindVertexArray(0);
    }
};

unsigned int TextureFromFile(const char* path, const std::string& directory);

class Model {
public:
    std::vector<Texture> textures_loaded;
    std::vector<Mesh> meshes;
    std::string directory;
    const aiScene* scene_ptr = nullptr;

    Model(const std::string& path) {
        loadModel(path);
    }

    void Draw(Shader& shader) {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

private:
    Assimp::Importer importer;

    void loadModel(const std::string& path) {
        scene_ptr = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs);

        if (!scene_ptr || scene_ptr->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene_ptr->mRootNode) {
            std::cout << "ASSIMP Error: " << importer.GetErrorString() << std::endl;
            return;
        }
        directory = path.substr(0, path.find_last_of('/'));
        if (directory == path) directory = path.substr(0, path.find_last_of('\\'));
        if (directory == path) directory = ".";

        processNode(scene_ptr->mRootNode, scene_ptr);
    }

    void processNode(aiNode* node, const aiScene* scene) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            if (mesh->HasNormals())
                vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            if (mesh->mTextureCoords[0])
                vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "diffuse", scene);
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        if (textures.empty()) {
            std::vector<Texture> baseColorMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "diffuse", scene);
            textures.insert(textures.end(), baseColorMaps.begin(), baseColorMaps.end());
        }

        return Mesh(vertices, indices, textures);
    }

    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene) {
        std::vector<Texture> textures;
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);
            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++) {
                if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                }
            }
            if (!skip) {
                Texture texture;

                const char* texPath = str.C_Str();
                if (texPath[0] == '*') {
                    int texIndex = atoi(texPath + 1);
                    if (scene->mTextures && texIndex < (int)scene->mNumTextures) {
                        texture.id = loadEmbeddedTexture(scene->mTextures[texIndex]);
                    }
                }
                else {
                    texture.id = TextureFromFile(str.C_Str(), this->directory);
                }

                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
        return textures;
    }

    unsigned int loadEmbeddedTexture(const aiTexture* tex) {
        unsigned int textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        int width, height, nrChannels;
        unsigned char* data = nullptr;

        if (tex->mHeight == 0) {
            data = stbi_load_from_memory(reinterpret_cast<unsigned char*>(tex->pcData),
                tex->mWidth, &width, &height, &nrChannels, 0);
        }
        else {
            width = tex->mWidth;
            height = tex->mHeight;
            nrChannels = 4;
            data = reinterpret_cast<unsigned char*>(tex->pcData);
        }

        if (data) {
            GLenum format = GL_RGBA;
            if (nrChannels == 1) format = GL_RED;
            else if (nrChannels == 3) format = GL_RGB;
            else if (nrChannels == 4) format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            if (tex->mHeight == 0) {
                stbi_image_free(data);
            }
        }

        return textureID;
    }
};

unsigned int TextureFromFile(const char* path, const std::string& directory) {
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        std::cout << "Texture failed to load at path: " << filename << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

unsigned int createFallbackTexture(unsigned char r, unsigned char g, unsigned char b) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    unsigned char pixels[4 * 4 * 3];
    for (int i = 0; i < 16; i++) {
        pixels[i * 3] = r;
        pixels[i * 3 + 1] = g;
        pixels[i * 3 + 2] = b;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

unsigned int loadTexture(const char* path, unsigned char fallbackR, unsigned char fallbackG, unsigned char fallbackB) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrChannels == 1) format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else if (nrChannels == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    }
    else {
        std::cout << "Texture not found: " << path << ", using fallback color" << std::endl;
        glDeleteTextures(1, &textureID);
        return createFallbackTexture(fallbackR, fallbackG, fallbackB);
    }

    return textureID;
}

void generateCylinder(std::vector<float>& vertices, std::vector<unsigned int>& indices,
    float radius, float height, int segments, int rings) {
    for (int r = 0; r <= rings; r++) {
        float y = (float)r / rings * height - height / 2;
        for (int s = 0; s <= segments; s++) {
            float angle = (float)s / segments * 2.0f * 3.14159f;
            float x = cos(angle) * radius;
            float z = sin(angle) * radius;

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(cos(angle));
            vertices.push_back(0.0f);
            vertices.push_back(sin(angle));
            vertices.push_back((float)s / segments);
            vertices.push_back((float)r / rings);
        }
    }

    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            int current = r * (segments + 1) + s;
            int next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    int baseIndex = vertices.size() / 8;

    vertices.push_back(0.0f);
    vertices.push_back(height / 2);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);
    int topCenter = baseIndex++;

    for (int s = 0; s <= segments; s++) {
        float angle = (float)s / segments * 2.0f * 3.14159f;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        vertices.push_back(x);
        vertices.push_back(height / 2);
        vertices.push_back(z);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(cos(angle) * 0.5f + 0.5f);
        vertices.push_back(sin(angle) * 0.5f + 0.5f);
    }

    for (int s = 0; s < segments; s++) {
        indices.push_back(topCenter);
        indices.push_back(baseIndex + s);
        indices.push_back(baseIndex + s + 1);
    }

    baseIndex = vertices.size() / 8;

    vertices.push_back(0.0f);
    vertices.push_back(-height / 2);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);
    int bottomCenter = baseIndex++;

    for (int s = 0; s <= segments; s++) {
        float angle = (float)s / segments * 2.0f * 3.14159f;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        vertices.push_back(x);
        vertices.push_back(-height / 2);
        vertices.push_back(z);
        vertices.push_back(0.0f);
        vertices.push_back(-1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(cos(angle) * 0.5f + 0.5f);
        vertices.push_back(sin(angle) * 0.5f + 0.5f);
    }

    for (int s = 0; s < segments; s++) {
        indices.push_back(bottomCenter);
        indices.push_back(baseIndex + s + 1);
        indices.push_back(baseIndex + s);
    }
}

void generateHemisphere(std::vector<float>& vertices, std::vector<unsigned int>& indices,
    float radius, int segments, int rings) {
    for (int r = 0; r <= rings; r++) {
        float phi = (float)r / rings * 3.14159f / 2.0f;
        float y = sin(phi) * radius;
        float ringRadius = cos(phi) * radius;

        for (int s = 0; s <= segments; s++) {
            float theta = (float)s / segments * 2.0f * 3.14159f;
            float x = cos(theta) * ringRadius;
            float z = sin(theta) * ringRadius;

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
            vertices.push_back((float)s / segments);
            vertices.push_back((float)r / rings);
        }
    }

    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            int current = r * (segments + 1) + s;
            int next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    int baseIndex = vertices.size() / 8;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);
    int center = baseIndex++;

    for (int s = 0; s <= segments; s++) {
        float theta = (float)s / segments * 2.0f * 3.14159f;
        float x = cos(theta) * radius;
        float z = sin(theta) * radius;

        vertices.push_back(x);
        vertices.push_back(0.0f);
        vertices.push_back(z);
        vertices.push_back(0.0f);
        vertices.push_back(-1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(cos(theta) * 0.5f + 0.5f);
        vertices.push_back(sin(theta) * 0.5f + 0.5f);
    }

    for (int s = 0; s < segments; s++) {
        indices.push_back(center);
        indices.push_back(baseIndex + s + 1);
        indices.push_back(baseIndex + s);
    }
}

void generateDomePlatform(std::vector<float>& vertices, std::vector<unsigned int>& indices,
    float radius, float depth, int segments, int rings) {
    for (int r = 0; r <= rings; r++) {
        float phi = (float)r / rings * 3.14159f / 2.0f;
        float y = -sin(phi) * depth;
        float ringRadius = cos(phi) * radius;

        for (int s = 0; s <= segments; s++) {
            float theta = (float)s / segments * 2.0f * 3.14159f;
            float x = cos(theta) * ringRadius;
            float z = sin(theta) * ringRadius;

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            glm::vec3 normal = glm::normalize(glm::vec3(-x * depth / radius, radius, -z * depth / radius));
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            vertices.push_back((float)s / segments);
            vertices.push_back((float)r / rings);
        }
    }

    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            int current = r * (segments + 1) + s;
            int next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);

            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
        }
    }

    int baseIndex = vertices.size() / 8;

    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);
    int topCenter = baseIndex++;

    for (int s = 0; s <= segments; s++) {
        float theta = (float)s / segments * 2.0f * 3.14159f;
        float x = cos(theta) * radius;
        float z = sin(theta) * radius;

        vertices.push_back(x);
        vertices.push_back(0.0f);
        vertices.push_back(z);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(cos(theta) * 0.5f + 0.5f);
        vertices.push_back(sin(theta) * 0.5f + 0.5f);
    }

    for (int s = 0; s < segments; s++) {
        indices.push_back(topCenter);
        indices.push_back(baseIndex + s);
        indices.push_back(baseIndex + s + 1);
    }
}

void generateComplexCylinder(std::vector<float>& vertices, std::vector<unsigned int>& indices,
    float radius, float height, int segments, int rings, int twists) {
    for (int r = 0; r <= rings; r++) {
        float y = (float)r / rings * height - height / 2;
        float twistAngle = (float)r / rings * twists * 2.0f * 3.14159f;
        float waveRadius = radius + sin((float)r / rings * 8.0f * 3.14159f) * 0.15f * radius;

        for (int s = 0; s <= segments; s++) {
            float angle = (float)s / segments * 2.0f * 3.14159f + twistAngle;

            float starFactor = 1.0f + 0.2f * sin(angle * 6.0f);
            float finalRadius = waveRadius * starFactor;

            float x = cos(angle) * finalRadius;
            float z = sin(angle) * finalRadius;

            glm::vec3 normal = glm::normalize(glm::vec3(cos(angle) * starFactor, 0.0f, sin(angle) * starFactor));

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
            vertices.push_back((float)s / segments * 3.0f);
            vertices.push_back((float)r / rings * 2.0f);
        }
    }

    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            int current = r * (segments + 1) + s;
            int next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    int baseIndex = vertices.size() / 8;
    vertices.push_back(0.0f);
    vertices.push_back(height / 2);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);
    int topCenter = baseIndex++;

    float twistTop = (float)rings / rings * twists * 2.0f * 3.14159f;
    for (int s = 0; s <= segments; s++) {
        float angle = (float)s / segments * 2.0f * 3.14159f + twistTop;
        float starFactor = 1.0f + 0.2f * sin(angle * 6.0f);
        float x = cos(angle) * radius * starFactor;
        float z = sin(angle) * radius * starFactor;

        vertices.push_back(x);
        vertices.push_back(height / 2);
        vertices.push_back(z);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(cos(angle) * 0.5f + 0.5f);
        vertices.push_back(sin(angle) * 0.5f + 0.5f);
    }

    for (int s = 0; s < segments; s++) {
        indices.push_back(topCenter);
        indices.push_back(baseIndex + s);
        indices.push_back(baseIndex + s + 1);
    }

    baseIndex = vertices.size() / 8;
    vertices.push_back(0.0f);
    vertices.push_back(-height / 2);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);
    int bottomCenter = baseIndex++;

    for (int s = 0; s <= segments; s++) {
        float angle = (float)s / segments * 2.0f * 3.14159f;
        float starFactor = 1.0f + 0.2f * sin(angle * 6.0f);
        float x = cos(angle) * radius * starFactor;
        float z = sin(angle) * radius * starFactor;

        vertices.push_back(x);
        vertices.push_back(-height / 2);
        vertices.push_back(z);
        vertices.push_back(0.0f);
        vertices.push_back(-1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(cos(angle) * 0.5f + 0.5f);
        vertices.push_back(sin(angle) * 0.5f + 0.5f);
    }

    for (int s = 0; s < segments; s++) {
        indices.push_back(bottomCenter);
        indices.push_back(baseIndex + s + 1);
        indices.push_back(baseIndex + s);
    }
}

float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

unsigned int loadCubemap(std::vector<std::string> faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = GL_RGB;
            if (nrChannels == 4) format = GL_RGBA;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

void generateTree(std::vector<float>& vertices, std::vector<unsigned int>& indices,
    float trunkRadius, float trunkHeight, float crownRadius, float crownHeight, int segments) {
    int baseIndex = vertices.size() / 8;

    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 2.0f * 3.14159f;
        float x = cos(angle) * trunkRadius;
        float z = sin(angle) * trunkRadius;

        vertices.push_back(x);
        vertices.push_back(0.0f);
        vertices.push_back(z);
        vertices.push_back(cos(angle));
        vertices.push_back(0.0f);
        vertices.push_back(sin(angle));
        vertices.push_back((float)i / segments);
        vertices.push_back(0.0f);

        vertices.push_back(x);
        vertices.push_back(trunkHeight);
        vertices.push_back(z);
        vertices.push_back(cos(angle));
        vertices.push_back(0.0f);
        vertices.push_back(sin(angle));
        vertices.push_back((float)i / segments);
        vertices.push_back(1.0f);
    }

    for (int i = 0; i < segments; i++) {
        int bottom1 = baseIndex + i * 2;
        int top1 = bottom1 + 1;
        int bottom2 = bottom1 + 2;
        int top2 = bottom2 + 1;

        indices.push_back(bottom1);
        indices.push_back(top1);
        indices.push_back(bottom2);

        indices.push_back(bottom2);
        indices.push_back(top1);
        indices.push_back(top2);
    }

    int crownBase = vertices.size() / 8;
    int layers = 3;

    for (int layer = 0; layer < layers; layer++) {
        float layerY = trunkHeight + layer * crownHeight * 0.3f;
        float layerRadius = crownRadius * (1.0f - layer * 0.25f);
        float layerTopY = layerY + crownHeight * 0.45f;

        int layerBase = vertices.size() / 8;

        vertices.push_back(0.0f);
        vertices.push_back(layerY);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(-1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(0.5f);
        vertices.push_back(0.0f);

        vertices.push_back(0.0f);
        vertices.push_back(layerTopY);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
        vertices.push_back(0.5f);
        vertices.push_back(1.0f);

        for (int i = 0; i <= segments; i++) {
            float angle = (float)i / segments * 2.0f * 3.14159f;
            float x = cos(angle) * layerRadius;
            float z = sin(angle) * layerRadius;

            glm::vec3 normal = glm::normalize(glm::vec3(cos(angle), 0.5f, sin(angle)));

            vertices.push_back(x);
            vertices.push_back(layerY + crownHeight * 0.15f);
            vertices.push_back(z);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
            vertices.push_back(cos(angle) * 0.5f + 0.5f);
            vertices.push_back(sin(angle) * 0.5f + 0.5f);
        }

        int bottomCenter = layerBase;
        int topCenter = layerBase + 1;
        int ringStart = layerBase + 2;

        for (int i = 0; i < segments; i++) {
            indices.push_back(ringStart + i);
            indices.push_back(topCenter);
            indices.push_back(ringStart + i + 1);

            indices.push_back(ringStart + i + 1);
            indices.push_back(bottomCenter);
            indices.push_back(ringStart + i);
        }
    }
}

int getBiomeRegion(float xPos, float zPos) {
    float angle = atan2(zPos, xPos);
    float angleDeg = angle * 180.0f / 3.14159f;
    if (angleDeg < 0) angleDeg += 360.0f;

    if (angleDeg >= 210.0f && angleDeg < 330.0f) {
        return 0;
    }
    else if (angleDeg >= 90.0f && angleDeg < 210.0f) {
        return 1;
    }
    else {
        return 2;
    }
}

void getBiomeWeights(float xPos, float zPos, float& grassWeight, float& rockWeight, float& snowWeight) {
    float angle = atan2(zPos, xPos);
    float angleDeg = angle * 180.0f / 3.14159f;
    if (angleDeg < 0) angleDeg += 360.0f;

    float transitionWidth = 25.0f;

    grassWeight = 0.0f;
    rockWeight = 0.0f;
    snowWeight = 0.0f;

    float grassDist = std::min(abs(angleDeg - 270.0f), 360.0f - abs(angleDeg - 270.0f));
    float rockDist = std::min(abs(angleDeg - 150.0f), 360.0f - abs(angleDeg - 150.0f));
    float snowDist = std::min(abs(angleDeg - 30.0f), abs(angleDeg - 390.0f));
    if (angleDeg > 180.0f) snowDist = std::min(snowDist, abs(angleDeg - 390.0f));
    else snowDist = std::min(snowDist, abs(angleDeg - 30.0f));

    float grassRange = 60.0f + transitionWidth;
    float rockRange = 60.0f + transitionWidth;
    float snowRange = 60.0f + transitionWidth;

    if (grassDist < grassRange) {
        grassWeight = 1.0f - (grassDist / grassRange);
        grassWeight = grassWeight * grassWeight * (3.0f - 2.0f * grassWeight);
    }
    if (rockDist < rockRange) {
        rockWeight = 1.0f - (rockDist / rockRange);
        rockWeight = rockWeight * rockWeight * (3.0f - 2.0f * rockWeight);
    }
    if (snowDist < snowRange) {
        snowWeight = 1.0f - (snowDist / snowRange);
        snowWeight = snowWeight * snowWeight * (3.0f - 2.0f * snowWeight);
    }

    float total = grassWeight + rockWeight + snowWeight + 0.001f;
    grassWeight /= total;
    rockWeight /= total;
    snowWeight /= total;
}

float getBlendedTerrainHeight(int x, int z, float xPos, float zPos, float heightScale) {
    float grassWeight, rockWeight, snowWeight;
    getBiomeWeights(xPos, zPos, grassWeight, rockWeight, snowWeight);

    float grassBase = perlin.fbm(x * 0.03f, z * 0.03f, 4, 0.4f) * heightScale * 0.4f;
    float grassDetail = perlin.fbm(x * 0.1f + 100, z * 0.1f + 100, 3, 0.3f) * heightScale * 0.1f;
    float grassHeight = grassBase + grassDetail - 5.0f;

    float rockBase = perlin.fbm(x * 0.04f + 50, z * 0.04f + 50, 5, 0.55f) * heightScale * 0.9f;
    float rockDetail = perlin.fbm(x * 0.15f + 200, z * 0.15f + 200, 4, 0.5f) * heightScale * 0.3f;
    float ridges = abs(perlin.fbm(x * 0.08f + 300, z * 0.08f + 300, 3, 0.6f)) * heightScale * 0.4f;
    float rockHeight = rockBase + rockDetail + ridges + 4.0f;

    float snowBase = perlin.fbm(x * 0.035f + 150, z * 0.035f + 150, 6, 0.5f) * heightScale * 1.5f;
    float peaks = pow(abs(perlin.fbm(x * 0.06f + 400, z * 0.06f + 400, 4, 0.6f)), 1.5f) * heightScale * 0.8f;
    float snowDetail = perlin.fbm(x * 0.12f + 500, z * 0.12f + 500, 3, 0.4f) * heightScale * 0.2f;
    float snowHeight = snowBase + peaks + snowDetail + 13.0f;

    return grassHeight * grassWeight + rockHeight * rockWeight + snowHeight * snowWeight;
}

void generateTerrain(std::vector<float>& vertices, std::vector<unsigned int>& indices,
    int width, int depth, float scale, float heightScale) {
    for (int z = 0; z <= depth; z++) {
        for (int x = 0; x <= width; x++) {
            float xPos = (x - width / 2.0f) * scale;
            float zPos = (z - depth / 2.0f) * scale;

            float height = getBlendedTerrainHeight(x, z, xPos, zPos, heightScale);

            vertices.push_back(xPos);
            vertices.push_back(height);
            vertices.push_back(zPos);

            auto getHeight = [&](int px, int pz) -> float {
                float pxPos = (px - width / 2.0f) * scale;
                float pzPos = (pz - depth / 2.0f) * scale;
                return getBlendedTerrainHeight(px, pz, pxPos, pzPos, heightScale);
                };

            float hL = getHeight(x - 1, z);
            float hR = getHeight(x + 1, z);
            float hD = getHeight(x, z - 1);
            float hU = getHeight(x, z + 1);

            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * scale, hD - hU));
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            vertices.push_back(xPos);
            vertices.push_back(zPos);
        }
    }

    for (int z = 0; z < depth; z++) {
        for (int x = 0; x < width; x++) {
            int topLeft = z * (width + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (width + 1) + x;
            int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
}

void generatePlatform(std::vector<float>& vertices, std::vector<unsigned int>& indices,
    float width, float height, float depth) {
    float hw = width / 2, hh = height / 2, hd = depth / 2;

    int base = vertices.size() / 8;
    float platformVerts[] = {
        -hw, hh, -hd,       0, 1, 0,         0, 0,
         hw, hh, -hd,       0, 1, 0,         1, 0,
         hw, hh,  hd,       0, 1, 0,         1, 1,
        -hw, hh,  hd,       0, 1, 0,         0, 1,
        -hw, -hh, -hd,      0, -1, 0,        0, 0,
         hw, -hh, -hd,      0, -1, 0,        1, 0,
         hw, -hh,  hd,      0, -1, 0,        1, 1,
        -hw, -hh,  hd,      0, -1, 0,        0, 1,
        -hw, -hh, hd,       0, 0, 1,         0, 0,
         hw, -hh, hd,       0, 0, 1,         1, 0,
         hw,  hh, hd,       0, 0, 1,         1, 1,
        -hw,  hh, hd,       0, 0, 1,         0, 1,
        -hw, -hh, -hd,      0, 0, -1,        0, 0,
         hw, -hh, -hd,      0, 0, -1,        1, 0,
         hw,  hh, -hd,      0, 0, -1,        1, 1,
        -hw,  hh, -hd,      0, 0, -1,        0, 1,
        -hw, -hh, -hd,      -1, 0, 0,        0, 0,
        -hw, -hh,  hd,      -1, 0, 0,        1, 0,
        -hw,  hh,  hd,      -1, 0, 0,        1, 1,
        -hw,  hh, -hd,      -1, 0, 0,        0, 1,
         hw, -hh, -hd,       1, 0, 0,        0, 0,
         hw, -hh,  hd,       1, 0, 0,        1, 0,
         hw,  hh,  hd,       1, 0, 0,        1, 1,
         hw,  hh, -hd,       1, 0, 0,        0, 1,
    };

    for (int i = 0; i < 24 * 8; i++) {
        vertices.push_back(platformVerts[i]);
    }

    unsigned int platformIndices[] = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        8, 9, 10, 8, 10, 11,
        12, 14, 13, 12, 15, 14,
        16, 17, 18, 16, 18, 19,
        20, 22, 21, 20, 23, 22
    };

    for (int i = 0; i < 36; i++) {
        indices.push_back(base + platformIndices[i]);
    }
}

static bool fKeyPressed = false;
static bool key1Pressed = false;
static bool key2Pressed = false;
static bool keyMPressed = false;
static bool keyPPressed = false;

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !key1Pressed) {
        key1Pressed = true;
        if (activeDebugModel == 1) {
            activeDebugModel = 0;
            std::cout << "Model debug: OFF" << std::endl;
        }
        else {
            activeDebugModel = 1;
            std::cout << "Model debug: forest_assets (various_forest_assets_pack.glb)" << std::endl;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) {
        key1Pressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !key2Pressed) {
        key2Pressed = true;
        if (activeDebugModel == 2) {
            activeDebugModel = 0;
            std::cout << "Model debug: OFF" << std::endl;
        }
        else {
            activeDebugModel = 2;
            std::cout << "Model debug: winter_tree (low_poly_winter_tree_pack.glb)" << std::endl;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) {
        key2Pressed = false;
    }

    if (activeDebugModel > 0) {
        float moveSpeed = 20.0f * deltaTime;
        float rotSpeed = 50.0f * deltaTime;
        float scaleSpeed = 1.0f * deltaTime;

        glm::vec3* pos = (activeDebugModel == 1) ? &forestPositions[0] : &winterPositions[0];
        glm::vec3* rot = (activeDebugModel == 1) ? &forestDebugRot : &winterDebugRot;
        float* scale = (activeDebugModel == 1) ? &forestDebugScale : &winterDebugScale;

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            pos->y += moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            pos->y -= moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            pos->x -= moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            pos->x += moveSpeed;

        if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
            pos->z += moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
            pos->z -= moveSpeed;

        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
            rot->x += rotSpeed;
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
            rot->x -= rotSpeed;

        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
            rot->y += rotSpeed;
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
            rot->y -= rotSpeed;

        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
            rot->z += rotSpeed;
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
            rot->z -= rotSpeed;

        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
            *scale += scaleSpeed;
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
            *scale = std::max(0.01f, *scale - scaleSpeed);

        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && !keyMPressed) {
            keyMPressed = true;
            const char* modelName = (activeDebugModel == 1) ? "forest_assets" : "winter_tree";
            std::cout << "\n=== " << modelName << " Transform ===" << std::endl;
            std::cout << "Position: glm::vec3(" << pos->x << "f, " << pos->y << "f, " << pos->z << "f)" << std::endl;
            std::cout << "Rotation: glm::vec3(" << rot->x << "f, " << rot->y << "f, " << rot->z << "f)" << std::endl;
            std::cout << "Scale: " << *scale << "f" << std::endl;
            std::cout << "========================\n" << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE) {
            keyMPressed = false;
        }
    }

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !keyPPressed) {
        keyPPressed = true;
        std::cout << "\n=== Camera Info ===" << std::endl;
        std::cout << "Position: glm::vec3(" << cameraPos.x << "f, " << cameraPos.y << "f, " << cameraPos.z << "f)" << std::endl;
        std::cout << "Front: glm::vec3(" << cameraFront.x << "f, " << cameraFront.y << "f, " << cameraFront.z << "f)" << std::endl;
        std::cout << "Yaw: " << yaw << "f, Pitch: " << pitch << "f" << std::endl;
        std::cout << "===================\n" << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) {
        keyPPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !fKeyPressed) {
        flyMode = !flyMode;
        fKeyPressed = true;
        std::cout << "Fly mode: " << (flyMode ? "ON" : "OFF") << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) {
        fKeyPressed = false;
    }

    isSprinting = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    float speed = playerSpeed * (isSprinting ? sprintMultiplier : 1.0f);

    glm::vec3 front = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 right = glm::normalize(glm::cross(front, cameraUp));

    glm::vec3 moveDir(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        moveDir += front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        moveDir -= front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        moveDir -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        moveDir += right;

    if (glm::length(moveDir) > 0.0f)
        moveDir = glm::normalize(moveDir);

    float newX = playerPos.x + moveDir.x * speed * deltaTime;
    float newZ = playerPos.z + moveDir.z * speed * deltaTime;

    float terrainBound = 195.0f;
    newX = std::max(-terrainBound, std::min(terrainBound, newX));
    newZ = std::max(-terrainBound, std::min(terrainBound, newZ));

    bool collisionX = false;
    bool collisionZ = false;
    float treeCollisionRadius = 2.5f;

    for (size_t i = 0; i < forestPositions.size(); i++) {
        glm::vec3 treePos = forestPositions[i];
        float treeCollisionHeight = 5.0f;
        if (playerPos.y > treePos.y + treeCollisionHeight) {
            continue;
        }
        float dxNew = newX - treePos.x;
        float dzOld = playerPos.z - treePos.z;
        if (sqrt(dxNew * dxNew + dzOld * dzOld) < treeCollisionRadius) {
            collisionX = true;
        }
        float dxOld = playerPos.x - treePos.x;
        float dzNew = newZ - treePos.z;
        if (sqrt(dxOld * dxOld + dzNew * dzNew) < treeCollisionRadius) {
            collisionZ = true;
        }
    }

    for (size_t i = 0; i < winterPositions.size(); i++) {
        glm::vec3 treePos = winterPositions[i];
        float dxNew = newX - treePos.x;
        float dzOld = playerPos.z - treePos.z;
        if (sqrt(dxNew * dxNew + dzOld * dzOld) < treeCollisionRadius) {
            collisionX = true;
        }
        float dxOld = playerPos.x - treePos.x;
        float dzNew = newZ - treePos.z;
        if (sqrt(dxOld * dxOld + dzNew * dzNew) < treeCollisionRadius) {
            collisionZ = true;
        }
    }

    if (!collisionX) {
        playerPos.x = newX;
    }
    if (!collisionZ) {
        playerPos.z = newZ;
    }

    if (flyMode) {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            playerPos.y += speed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            playerPos.y -= speed * deltaTime;
    }
    else {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded) {
            playerVelocity.y = jumpForce;
            isGrounded = false;
        }
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "game", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    soundEngine = createIrrKlangDevice();
    if (soundEngine) {
        ISound* bgMusic = soundEngine->play2D("1.mp3", true, false, true);
        if (bgMusic) {
            bgMusic->setVolume(0.5f);
        }
    }

    Shader mainShader("shaders/vertex.glsl", "shaders/fragment.glsl");
    Shader uiShader("shaders/ui_vertex.glsl", "shaders/ui_fragment.glsl");
    Shader skyboxShader("shaders/skybox_vertex.glsl", "shaders/skybox_fragment.glsl");

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    std::vector<std::string> faces = {
        "skybox/right.jpg",
        "skybox/left.jpg",
        "skybox/top.jpg",
        "skybox/bottom.jpg",
        "skybox/front.jpg",
        "skybox/back.jpg"
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    Model* cubeModel = nullptr;
    try {
        cubeModel = new Model("cube/cube.obj");
        std::cout << "Loaded cube.obj model" << std::endl;
    }
    catch (...) {
        std::cout << "Warning: Could not load cube.obj model" << std::endl;
    }
    unsigned int cubeTexture = loadTexture("cube/texture_wood.jpg", 120, 80, 50);

    Model* movingPlatformModel = nullptr;
    try {
        movingPlatformModel = new Model("30.glb");
        std::cout << "Loaded 30.glb model for moving platform" << std::endl;
    }
    catch (...) {
        std::cout << "Warning: Could not load 30.glb model" << std::endl;
    }

    Model* winterTreeModel = nullptr;
    try {
        winterTreeModel = new Model("realistic_hd_black_poplar_65105.glb");
        std::cout << "Loaded low_poly_winter_tree_pack.glb for snow terrain" << std::endl;
    }
    catch (...) {
        std::cout << "Warning: Could not load low_poly_winter_tree_pack.glb" << std::endl;
    }

    Model* forestAssetsModel = nullptr;
    try {
        forestAssetsModel = new Model("low_poly_tree.glb");
        std::cout << "low_poly_tree.glb" << std::endl;
    }
    catch (...) {
        std::cout << "Warning: Could not load various_forest_assets_pack.glb" << std::endl;
    }

    unsigned int grassTexture = loadTexture("grass.jpg", 60, 150, 50);
    unsigned int rockTexture = loadTexture("rock.png", 120, 100, 80);
    unsigned int snowTexture = loadTexture("snow.png", 240, 245, 255);
    unsigned int platformTexture = loadTexture("platform.jpg", 150, 120, 100);
    std::vector<float> terrainVerts;
    std::vector<unsigned int> terrainIndices;
    generateTerrain(terrainVerts, terrainIndices, 200, 200, 2.0f, 10.0f);

    unsigned int terrainVAO, terrainVBO, terrainEBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glGenBuffers(1, &terrainEBO);

    glBindVertexArray(terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, terrainVerts.size() * sizeof(float), terrainVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, terrainIndices.size() * sizeof(unsigned int), terrainIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    std::vector<float> platformVerts;
    std::vector<unsigned int> platformIndices;
    generatePlatform(platformVerts, platformIndices, 5.0f, 0.5f, 5.0f);

    unsigned int platformVAO, platformVBO, platformEBO;
    glGenVertexArrays(1, &platformVAO);
    glGenBuffers(1, &platformVBO);
    glGenBuffers(1, &platformEBO);

    glBindVertexArray(platformVAO);
    glBindBuffer(GL_ARRAY_BUFFER, platformVBO);
    glBufferData(GL_ARRAY_BUFFER, platformVerts.size() * sizeof(float), platformVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, platformIndices.size() * sizeof(unsigned int), platformIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    std::vector<glm::vec3> platformPositions;
    std::vector<glm::vec3> platformBasePositions;
    std::vector<PlatformType> platformTypes;
    std::vector<float> platformRotations;
    std::vector<float> platformRotSpeeds;
    std::vector<float> platformMovePhases;

    float radius = 70.0f;
    int numPlatforms = 35;
    float baseHeight = 28.0f;

    for (int i = 0; i < numPlatforms; i++) {
        float angle = (float)i / numPlatforms * 2.0f * 3.14159f;
        float px = sin(angle) * radius;
        float pz = cos(angle) * radius;

        float terrainH = perlin.fbm((px / 2.0f + 50) * 0.05f, (pz / 2.0f + 50) * 0.05f, 6, 0.5f) * 10.0f;

        float platformH = terrainH + baseHeight + sin(angle * 3) * 5.0f + sin(angle * 7) * 2.0f;

        platformPositions.push_back(glm::vec3(px, platformH, pz));
        platformBasePositions.push_back(glm::vec3(px, platformH, pz));

        PlatformType ptype;
        if (i == 0) {
            ptype = PLATFORM_BOX;
        }
        else if (i % 6 == 1) {
            ptype = PLATFORM_HEMISPHERE;
        }
        else if (i % 6 == 2) {
            ptype = PLATFORM_ROTATING;
        }
        else if (i % 6 == 3) {
            ptype = PLATFORM_CYLINDER;
        }
        else if (i % 6 == 4 || i % 6 == 5) {
            ptype = PLATFORM_OBJ;
        }
        else if (i % 12 == 0 && i != 0) {
            ptype = PLATFORM_MOVING;
        }
        else {
            ptype = PLATFORM_BOX;
        }
        platformTypes.push_back(ptype);
        platformRotations.push_back(0.0f);
        platformRotSpeeds.push_back(0.5f + (i % 7) * 0.15f);
        platformMovePhases.push_back((float)i * 0.5f);
    }

    std::cout << "Platform game: " << platformPositions.size() << " platforms in a circle" << std::endl;
    std::cout << "Radius: " << radius << ", Start at angle 0" << std::endl;

    std::vector<float> domePlatformVerts;
    std::vector<unsigned int> domePlatformIndices;
    generateDomePlatform(domePlatformVerts, domePlatformIndices, 3.0f, 2.0f, 24, 12);

    unsigned int domePlatformVAO, domePlatformVBO, domePlatformEBO;
    glGenVertexArrays(1, &domePlatformVAO);
    glGenBuffers(1, &domePlatformVBO);
    glGenBuffers(1, &domePlatformEBO);

    glBindVertexArray(domePlatformVAO);
    glBindBuffer(GL_ARRAY_BUFFER, domePlatformVBO);
    glBufferData(GL_ARRAY_BUFFER, domePlatformVerts.size() * sizeof(float), domePlatformVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, domePlatformEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, domePlatformIndices.size() * sizeof(unsigned int), domePlatformIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    std::vector<float> cylinderPlatformVerts;
    std::vector<unsigned int> cylinderPlatformIndices;
    generateComplexCylinder(cylinderPlatformVerts, cylinderPlatformIndices, 2.0f, 2.0f, 24, 12, 1);

    unsigned int cylinderPlatformVAO, cylinderPlatformVBO, cylinderPlatformEBO;
    glGenVertexArrays(1, &cylinderPlatformVAO);
    glGenBuffers(1, &cylinderPlatformVBO);
    glGenBuffers(1, &cylinderPlatformEBO);

    glBindVertexArray(cylinderPlatformVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cylinderPlatformVBO);
    glBufferData(GL_ARRAY_BUFFER, cylinderPlatformVerts.size() * sizeof(float), cylinderPlatformVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cylinderPlatformEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, cylinderPlatformIndices.size() * sizeof(unsigned int), cylinderPlatformIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    std::vector<glm::vec3> collectiblePositions;
    std::vector<bool> collectibleActive;

    int collectibleCount = 0;
    for (int i = 0; i < numPlatforms; i++) {
        if (platformTypes[i] == PLATFORM_MOVING) continue;

        if (collectibleCount % 3 == 0) {
            glm::vec3 platPos = platformPositions[i];
            collectiblePositions.push_back(glm::vec3(platPos.x, platPos.y + 2.0f, platPos.z));
            collectibleActive.push_back(true);
        }
        collectibleCount++;
    }
    std::cout << "Collectibles: " << collectiblePositions.size() << " hemispheres" << std::endl;

    {
        float heightScale = 10.0f;
        for (float tx = 80.0f; tx <= 160.0f; tx += 25.0f) {
            for (float tz = 30.0f; tz <= 150.0f; tz += 25.0f) {
                float offsetX = (perlin.noise(tx * 0.1f, tz * 0.1f, 100.0f) - 0.5f) * 8.0f;
                float offsetZ = (perlin.noise(tx * 0.1f + 50, tz * 0.1f + 50, 100.0f) - 0.5f) * 8.0f;
                float finalX = tx + offsetX;
                float finalZ = tz + offsetZ;
                float gridX = finalX / 2.0f + 100.0f;
                float gridZ = finalZ / 2.0f + 100.0f;
                float base = perlin.fbm(gridX * 0.035f + 150, gridZ * 0.035f + 150, 6, 0.5f) * heightScale * 1.5f;
                float peaks = pow(abs(perlin.fbm(gridX * 0.06f + 400, gridZ * 0.06f + 400, 4, 0.6f)), 1.5f) * heightScale * 0.8f;
                float detail = perlin.fbm(gridX * 0.12f + 500, gridZ * 0.12f + 500, 3, 0.4f) * heightScale * 0.2f;
                float terrainH = base + peaks + detail + 13.0f;
                winterPositions.push_back(glm::vec3(finalX, terrainH, finalZ));
            }
        }
    }
    std::cout << "Winter trees placed on snow terrain: " << winterPositions.size() << " instances" << std::endl;

    {
        float heightScale = 10.0f;
        for (float tx = -180.0f; tx <= 180.0f; tx += 42.0f) {
            for (float tz = -180.0f; tz <= 180.0f; tz += 42.0f) {
                if (getBiomeRegion(tx, tz) == 0) {
                    float offsetX = (perlin.noise(tx * 0.1f, tz * 0.1f, 0.0f) - 0.5f) * 13.0f;
                    float offsetZ = (perlin.noise(tx * 0.1f + 50, tz * 0.1f + 50, 0.0f) - 0.5f) * 13.0f;
                    float finalX = tx + offsetX;
                    float finalZ = tz + offsetZ;

                    float gridX = finalX / 2.0f + 100.0f;
                    float gridZ = finalZ / 2.0f + 100.0f;
                    float base = perlin.fbm(gridX * 0.03f, gridZ * 0.03f, 4, 0.4f) * heightScale * 0.4f;
                    float detail = perlin.fbm(gridX * 0.1f + 100, gridZ * 0.1f + 100, 3, 0.3f) * heightScale * 0.1f;
                    float terrainH = base + detail - 5.0f;
                    forestPositions.push_back(glm::vec3(finalX, terrainH, finalZ));
                }
            }
        }
    }
    std::cout << "Forest assets placed on grass terrain: " << forestPositions.size() << " instances" << std::endl;

    std::vector<float> hemisphereVerts;
    std::vector<unsigned int> hemisphereIndices;
    generateHemisphere(hemisphereVerts, hemisphereIndices, 0.8f, 16, 8);

    unsigned int hemisphereVAO, hemisphereVBO, hemisphereEBO;
    glGenVertexArrays(1, &hemisphereVAO);
    glGenBuffers(1, &hemisphereVBO);
    glGenBuffers(1, &hemisphereEBO);

    glBindVertexArray(hemisphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hemisphereVBO);
    glBufferData(GL_ARRAY_BUFFER, hemisphereVerts.size() * sizeof(float), hemisphereVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hemisphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, hemisphereIndices.size() * sizeof(unsigned int), hemisphereIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    float scoreQuad[] = {
        -0.95f, 0.95f,  0.0f, 0.0f,
        -0.35f, 0.95f,  1.0f, 0.0f,
        -0.35f, 0.82f,  1.0f, 1.0f,
        -0.95f, 0.82f,  0.0f, 1.0f,
    };
    unsigned int scoreIndices[] = { 0, 1, 2, 0, 2, 3 };

    unsigned int scoreVAO, scoreVBO, scoreEBO;
    glGenVertexArrays(1, &scoreVAO);
    glGenBuffers(1, &scoreVBO);
    glGenBuffers(1, &scoreEBO);

    glBindVertexArray(scoreVAO);
    glBindBuffer(GL_ARRAY_BUFFER, scoreVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(scoreQuad), scoreQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scoreEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(scoreIndices), scoreIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    float signatureQuad[] = {
        0.5f, -0.7f,  0.0f, 0.0f,
        0.95f, -0.7f, 1.0f, 0.0f,
        0.95f, -0.95f, 1.0f, 1.0f,
        0.5f, -0.95f, 0.0f, 1.0f,
    };
    unsigned int signatureIndices[] = { 0, 1, 2, 0, 2, 3 };

    unsigned int signatureVAO, signatureVBO, signatureEBO;
    glGenVertexArrays(1, &signatureVAO);
    glGenBuffers(1, &signatureVBO);
    glGenBuffers(1, &signatureEBO);

    glBindVertexArray(signatureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, signatureVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(signatureQuad), signatureQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, signatureEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(signatureIndices), signatureIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    playerPos = glm::vec3(76.0722f, 20.2518f, 21.3804f);
    cameraFront = glm::vec3(-0.724606f, 0.109736f, 0.68037f);
    yaw = 1576.8f;
    pitch = 6.30011f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        for (size_t i = 0; i < platformPositions.size(); i++) {
            if (platformTypes[i] == PLATFORM_ROTATING) {
                platformRotations[i] += platformRotSpeeds[i] * deltaTime;
            }
            else if (platformTypes[i] == PLATFORM_MOVING) {
                float moveRange = 8.0f;
                float moveSpeed = 1.2f;
                float offset = sin(currentFrame * moveSpeed + platformMovePhases[i]) * moveRange;
                platformPositions[i].x = platformBasePositions[i].x + offset;
            }
        }

        if (!flyMode) {
            playerVelocity.y += gravity * deltaTime;
            playerPos.y += playerVelocity.y * deltaTime;

            float heightScale = 10.0f;
            int gridX = (int)((playerPos.x / 2.0f) + 100.0f);
            int gridZ = (int)((playerPos.z / 2.0f) + 100.0f);
            float terrainHeight = getBlendedTerrainHeight(gridX, gridZ, playerPos.x, playerPos.z, heightScale);

            float highestPlatform = terrainHeight;
            for (size_t i = 0; i < platformPositions.size(); i++) {
                glm::vec3 platPos = platformPositions[i];
                PlatformType ptype = platformTypes[i];

                float dx = playerPos.x - platPos.x;
                float dz = playerPos.z - platPos.z;
                float dist2D = sqrt(dx * dx + dz * dz);

                if (ptype == PLATFORM_BOX || ptype == PLATFORM_ROTATING) {
                    if (abs(dx) < 2.5f && abs(dz) < 2.5f) {
                        if (playerVelocity.y <= 0 && playerPos.y >= platPos.y - 1.0f && playerPos.y < platPos.y + 5.0f) {
                            highestPlatform = std::max(highestPlatform, platPos.y + 0.2f);
                        }
                    }
                }
                else if (ptype == PLATFORM_HEMISPHERE) {
                    float domeRadius = 3.0f;
                    float domeDepth = 2.0f;

                    if (dist2D < domeRadius) {
                        float normalizedDist = dist2D / domeRadius;
                        float surfaceDepth = sqrt(1.0f - normalizedDist * normalizedDist) * domeDepth;
                        float platformSurface = platPos.y - surfaceDepth;

                        if (playerVelocity.y <= 0 && playerPos.y >= platPos.y - domeDepth - 1.0f && playerPos.y < platPos.y + 2.0f) {
                            highestPlatform = std::max(highestPlatform, platformSurface);
                        }
                    }
                }
                else if (ptype == PLATFORM_CYLINDER) {
                    float cylRadius = 2.0f * 1.2f;
                    float cylHeight = 2.0f;

                    if (dist2D < cylRadius) {
                        float platformSurface = platPos.y + cylHeight / 2.0f;

                        if (playerVelocity.y <= 0 && playerPos.y >= platPos.y && playerPos.y < platPos.y + cylHeight + 2.0f) {
                            highestPlatform = std::max(highestPlatform, platformSurface);
                        }
                    }
                }
                else if (ptype == PLATFORM_OBJ) {
                    if (abs(dx) < 3.0f && abs(dz) < 3.0f) {
                        if (playerVelocity.y <= 0 && playerPos.y >= platPos.y - 1.0f && playerPos.y < platPos.y + 5.0f) {
                            highestPlatform = std::max(highestPlatform, platPos.y + 0.5f);
                        }
                    }
                }
                else if (ptype == PLATFORM_MOVING) {
                    if (abs(dx) < 5.0f && abs(dz) < 5.0f) {
                        if (playerVelocity.y <= 0 && playerPos.y >= platPos.y - 1.0f && playerPos.y < platPos.y + 5.0f) {
                            highestPlatform = std::max(highestPlatform, platPos.y + 2.0f);
                            float moveRange = 8.0f;
                            float moveSpeed = 1.2f;
                            float prevOffset = sin((currentFrame - deltaTime) * moveSpeed + platformMovePhases[i]) * moveRange;
                            float currOffset = sin(currentFrame * moveSpeed + platformMovePhases[i]) * moveRange;
                            float deltaMove = currOffset - prevOffset;
                            playerPos.x += deltaMove;
                        }
                    }
                }
            }

            if (playerPos.y < highestPlatform + 1.0f) {
                playerPos.y = highestPlatform + 1.0f;
                playerVelocity.y = 0.0f;
                isGrounded = true;
            }
            else {
                isGrounded = false;
            }
        }

        for (size_t i = 0; i < collectiblePositions.size(); i++) {
            if (collectibleActive[i]) {
                glm::vec3 colPos = collectiblePositions[i];
                float dist = glm::length(playerPos - colPos);
                if (dist < 2.0f) {
                    collectibleActive[i] = false;
                    playerScore++;
                    std::cout << "Score: " << playerScore << std::endl;
                    if (soundEngine) {
                        if (collectSound) {
                            collectSound->stop();
                            collectSound->drop();
                        }
                        collectSound = soundEngine->play2D("2.mp3", false, false, true);
                        if (collectSound) {
                            collectSound->setVolume(1.0f);
                        }
                        collectSoundTimer = 1.0f;
                    }
                }
            }
        }

        if (collectSound && collectSoundTimer > 0) {
            collectSoundTimer -= deltaTime;
            if (collectSoundTimer <= 0) {
                collectSound->stop();
                collectSound->drop();
                collectSound = nullptr;
            }
        }

        cameraPos = playerPos + glm::vec3(0.0f, 2.0f, 0.0f);

        glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(60.0f),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);

        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
        skyboxShader.setMat4("view", skyboxView);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        skyboxShader.setInt("skybox", 0);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        mainShader.use();
        mainShader.setMat4("view", view);
        mainShader.setMat4("projection", projection);
        mainShader.setVec3("viewPos", cameraPos);
        mainShader.setVec3("lightPos", glm::vec3(50.0f, 100.0f, 50.0f));
        mainShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 0.95f));
        mainShader.setFloat("time", currentFrame);

        mainShader.setBool("isTerrain", true);
        mainShader.setBool("useTexture", false);
        mainShader.setBool("useDynamicLight", false);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        mainShader.setInt("grassTexture", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rockTexture);
        mainShader.setInt("rockTexture", 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, snowTexture);
        mainShader.setInt("snowTexture", 2);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        mainShader.setMat4("model", model);

        glBindVertexArray(terrainVAO);
        glDrawElements(GL_TRIANGLES, terrainIndices.size(), GL_UNSIGNED_INT, 0);

        mainShader.setBool("isTerrain", false);

        mainShader.setBool("useTexture", true);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, platformTexture);
        mainShader.setInt("texture_diffuse1", 0);

        for (size_t i = 0; i < platformPositions.size(); i++) {
            glm::vec3 pos = platformPositions[i];
            PlatformType ptype = platformTypes[i];

            model = glm::mat4(1.0f);
            model = glm::translate(model, pos);

            if (ptype == PLATFORM_OBJ && cubeModel) {
                model = glm::scale(model, glm::vec3(3.0f));
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cubeTexture);
                mainShader.setMat4("model", model);
                cubeModel->Draw(mainShader);
            }
            else if (ptype == PLATFORM_MOVING && movingPlatformModel) {
                model = glm::scale(model, glm::vec3(8.0f));
                mainShader.setBool("useTexture", true);
                mainShader.setMat4("model", model);
                movingPlatformModel->Draw(mainShader);
            }
            else {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, platformTexture);

                if (ptype == PLATFORM_BOX) {
                    glBindVertexArray(platformVAO);
                    mainShader.setMat4("model", model);
                    glDrawElements(GL_TRIANGLES, platformIndices.size(), GL_UNSIGNED_INT, 0);
                }
                else if (ptype == PLATFORM_HEMISPHERE) {
                    glBindVertexArray(domePlatformVAO);
                    mainShader.setMat4("model", model);
                    glDrawElements(GL_TRIANGLES, domePlatformIndices.size(), GL_UNSIGNED_INT, 0);
                }
                else if (ptype == PLATFORM_ROTATING) {
                    model = glm::rotate(model, platformRotations[i], glm::vec3(0.0f, 1.0f, 0.0f));
                    glBindVertexArray(platformVAO);
                    mainShader.setMat4("model", model);
                    glDrawElements(GL_TRIANGLES, platformIndices.size(), GL_UNSIGNED_INT, 0);
                }
                else if (ptype == PLATFORM_CYLINDER) {
                    glBindVertexArray(cylinderPlatformVAO);
                    mainShader.setMat4("model", model);
                    glDrawElements(GL_TRIANGLES, cylinderPlatformIndices.size(), GL_UNSIGNED_INT, 0);
                }
            }
        }

        mainShader.setBool("isTerrain", false);
        mainShader.setBool("useTexture", false);
        mainShader.setBool("useDynamicLight", true);
        mainShader.setVec3("objectColor", glm::vec3(1.0f, 0.8f, 0.2f));

        glBindVertexArray(hemisphereVAO);
        for (size_t i = 0; i < collectiblePositions.size(); i++) {
            if (collectibleActive[i]) {
                model = glm::mat4(1.0f);
                model = glm::translate(model, collectiblePositions[i]);
                model = glm::rotate(model, currentFrame * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
                mainShader.setMat4("model", model);
                glDrawElements(GL_TRIANGLES, hemisphereIndices.size(), GL_UNSIGNED_INT, 0);
            }
        }

        if (winterTreeModel) {
            mainShader.setBool("isTerrain", false);
            mainShader.setBool("useDynamicLight", false);
            mainShader.setBool("useTexture", true);
            for (size_t wi = 0; wi < winterPositions.size(); wi++) {
                model = glm::mat4(1.0f);
                model = glm::translate(model, winterPositions[wi]);
                model = glm::rotate(model, glm::radians(winterDebugRot.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(winterDebugRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(winterDebugRot.z), glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(winterDebugScale));
                mainShader.setMat4("model", model);
                winterTreeModel->Draw(mainShader);
            }
        }

        if (forestAssetsModel) {
            mainShader.setBool("isTerrain", false);
            mainShader.setBool("useDynamicLight", false);
            mainShader.setBool("useTexture", true);
            for (size_t fi = 0; fi < forestPositions.size(); fi++) {
                model = glm::mat4(1.0f);
                model = glm::translate(model, forestPositions[fi]);
                model = glm::rotate(model, glm::radians(forestDebugRot.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(forestDebugRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(forestDebugRot.z), glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(forestDebugScale));
                mainShader.setMat4("model", model);
                forestAssetsModel->Draw(mainShader);
            }
        }

        glDisable(GL_DEPTH_TEST);
        uiShader.use();

        uiShader.setBool("hasTexture", false);
        uiShader.setBool("isScoreDisplay", true);
        uiShader.setInt("displayScore", playerScore);
        float scoreIntensity = 0.7f + (float)playerScore / 30.0f;
        uiShader.setVec3("textColor", glm::vec3(scoreIntensity, scoreIntensity * 0.85f, 0.3f));
        glBindVertexArray(scoreVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        uiShader.setBool("isScoreDisplay", false);
        uiShader.setVec3("textColor", glm::vec3(1.0f, 1.0f, 1.0f));
        glBindVertexArray(signatureVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &terrainEBO);

    glDeleteVertexArrays(1, &platformVAO);
    glDeleteBuffers(1, &platformVBO);
    glDeleteBuffers(1, &platformEBO);

    glDeleteVertexArrays(1, &domePlatformVAO);
    glDeleteBuffers(1, &domePlatformVBO);
    glDeleteBuffers(1, &domePlatformEBO);

    glDeleteVertexArrays(1, &cylinderPlatformVAO);
    glDeleteBuffers(1, &cylinderPlatformVBO);
    glDeleteBuffers(1, &cylinderPlatformEBO);

    glDeleteVertexArrays(1, &hemisphereVAO);
    glDeleteBuffers(1, &hemisphereVBO);
    glDeleteBuffers(1, &hemisphereEBO);

    glDeleteVertexArrays(1, &scoreVAO);
    glDeleteBuffers(1, &scoreVBO);
    glDeleteBuffers(1, &scoreEBO);

    glDeleteVertexArrays(1, &signatureVAO);
    glDeleteBuffers(1, &signatureVBO);
    glDeleteBuffers(1, &signatureEBO);

    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);

    if (cubeModel) delete cubeModel;
    if (movingPlatformModel) delete movingPlatformModel;
    if (winterTreeModel) delete winterTreeModel;
    if (forestAssetsModel) delete forestAssetsModel;
    if (soundEngine) soundEngine->drop();

    glfwTerminate();
    return 0;
}