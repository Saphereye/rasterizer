#include "assimp/scene.h"
#include <iostream>
#include <ostream>

struct VertexFormat
{
    bool hasNormals;
    bool hasTexCoords;
    int  stride; // floats per vertex
};

VertexFormat analyzeScene(const aiScene* scene)
{
    VertexFormat fmt = { false, false, 3 }; // minimum: position only

    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[i];
        if (mesh->HasNormals()) fmt.hasNormals = true;
        if (mesh->HasTextureCoords(0)) fmt.hasTexCoords = true;
    }

    fmt.stride = 3; // position
    if (fmt.hasNormals) fmt.stride += 3;
    if (fmt.hasTexCoords) fmt.stride += 2;

    std::cout << "Scene format: pos=3, normals=" << (fmt.hasNormals ? 3 : 0)
              << ", texcoords=" << (fmt.hasTexCoords ? 2 : 0)
              << ", stride=" << fmt.stride << std::endl;

    return fmt;
}

void extractVertices(aiNode*             node,
                     const aiScene*      scene,
                     std::vector<float>& vertices)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        std::cout << "Processing mesh " << i << ": " << mesh->mNumFaces
                  << " faces, " << mesh->mNumVertices << " vertices"
                  << std::endl;

        // Process each face and extract vertices in order
        for (unsigned int j = 0; j < mesh->mNumFaces; j++)
        {
            aiFace face = mesh->mFaces[j];

            // Each face should be a triangle (due to aiProcess_Triangulate)
            for (unsigned int k = 0; k < face.mNumIndices; k++)
            {
                unsigned int vertexIndex = face.mIndices[k];

                // Position
                aiVector3D pos = mesh->mVertices[vertexIndex];
                vertices.push_back(pos.x);
                vertices.push_back(pos.y);
                vertices.push_back(pos.z);

                // Normal (with fallback)
                if (mesh->HasNormals())
                {
                    aiVector3D normal = mesh->mNormals[vertexIndex];
                    vertices.push_back(normal.x);
                    vertices.push_back(normal.y);
                    vertices.push_back(normal.z);
                }
                else
                {
                    // Generate a simple face normal or use default
                    vertices.push_back(0.0f);
                    vertices.push_back(1.0f);
                    vertices.push_back(0.0f);
                }
            }
        }
    }

    // Process child nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        extractVertices(node->mChildren[i], scene, vertices);
    }
}
