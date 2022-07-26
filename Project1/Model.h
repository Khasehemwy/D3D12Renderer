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
        ID3D12Device* pDevice,
        ID3D12GraphicsCommandList* pCommandList)
    {
        mGeo.Name = name;
        LoadModel(path, pDevice, pCommandList);
    }

    const MeshGeometry* Geo();

private:
    /*  模型数据  */
    MeshGeometry mGeo;
    vector<GeometryGenerator::MeshData> mMeshes;
    string mDirectory;

    void LoadModel(
        string path,
        ID3D12Device* pDevice,
        ID3D12GraphicsCommandList* pCommandList);

    void ProcessGeo(
        ID3D12Device* pDevice,
        ID3D12GraphicsCommandList* pCommandList);

    void ProcessNode(aiNode* node, const aiScene* scene);
    GeometryGenerator::MeshData ProcessMesh(aiMesh* mesh, const aiScene* scene);
};