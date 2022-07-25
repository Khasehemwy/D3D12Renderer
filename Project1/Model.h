#pragma once

#include <vector>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "../../Common/d3dApp.h"
#include "../../Common/GeometryGenerator.h"

using std::vector;
using std::string;

class Model
{
public:
    Model(
        string name, 
        string path,
        std::unique_ptr<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> pCommandList)
    {
        mpCommandList = std::move(pCommandList);
        mGeo.Name = name;
        LoadModel(path);
    }

private:
    /*  模型数据  */
    MeshGeometry mGeo;
    vector<GeometryGenerator::MeshData> mMeshes;
    string mDirectory;
    std::unique_ptr<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> mpCommandList;

    void LoadModel(string path);
    void ProcessGeo();
    void ProcessNode(aiNode* node, const aiScene* scene);
    GeometryGenerator::MeshData ProcessMesh(aiMesh* mesh, const aiScene* scene);
};