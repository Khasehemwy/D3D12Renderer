#pragma once
#include "Model.h"
#include "../../Common/MathHelper.h"
#include <iostream>

void Model::LoadModel(string path)
{
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
		return;
	}

	mDirectory = path.substr(0, path.find_last_of('/'));

	ProcessNode(scene->mRootNode, scene);

	ProcessGeo();
}

void Model::ProcessGeo()
{
	int totalVertexCount = 0;
	for (auto& meshData : mMeshes) {
		totalVertexCount += meshData.Vertices.size();
	}

	std::vector<GeometryGenerator::Vertex> vertices(totalVertexCount);
	UINT k = 0;
	for (auto& meshData : mMeshes) {
		for (size_t i = 0; i < meshData.Vertices.size(); ++i, ++k)
		{
			vertices[k].pos = box.Vertices[i].Position;
			vertices[k].color = XMFLOAT4(DirectX::Colors::DarkGreen);
		}
	}


	const UINT vbByteSize = (UINT)m.size() * sizeof(GeometryGenerator::Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU);
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU);
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
}

void Model::ProcessNode(aiNode* node, const aiScene* scene)
{
	// 处理节点所有的网格（如果有的话）
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		mMeshes.push_back(ProcessMesh(mesh, scene));
	}
	// 接下来对它的子节点重复这一过程
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene);
	}
}

GeometryGenerator::MeshData Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
	using namespace DirectX;

	GeometryGenerator::MeshData meshData;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		GeometryGenerator::Vertex vertex;
		// 处理顶点位置、法线和纹理坐标
		XMFLOAT3 vector;
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.Position = vector;

		if (mesh->mNormals) {
			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
		}
		vertex.Normal = vector;

		if (mesh->mTextureCoords[0]) // 网格是否有纹理坐标？
		{
			XMFLOAT2 vec;
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.TexC = vec;
		}
		else {
			vertex.TexC = XMFLOAT2(0.0f, 0.0f);
		}

		meshData.Vertices.push_back(vertex);
	}
	// 处理索引
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++) {
			meshData.Indices32.push_back(face.mIndices[j]);
		}
	}

	// 处理材质
	if (mesh->mMaterialIndex >= 0)
	{
		//aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		//vector<Texture> diffuseMaps = loadMaterialTextures(material,
		//	aiTextureType_DIFFUSE, "texture_diffuse");
		//textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		//vector<Texture> specularMaps = loadMaterialTextures(material,
		//	aiTextureType_SPECULAR, "texture_specular");
		//textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
	}

	return meshData;
}
