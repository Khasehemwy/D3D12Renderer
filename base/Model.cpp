#pragma once
#include "Model.h"

#include <iostream>

const MeshGeometry* Model::Geo()
{
	return &mGeo;
}

void Model::LoadModel(
	string path,
	ID3D12Device* pDevice,
	ID3D12GraphicsCommandList* pCommandList)
{
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		string err = (string)"ERROR::ASSIMP::" + import.GetErrorString() + "\n";
		throw std::invalid_argument(err);
	}

	mDirectory = path.substr(0, path.find_last_of('/'));

	ProcessNode(scene->mRootNode, scene);

	ProcessGeo(pDevice, pCommandList);
}

void Model::ProcessGeo(
	ID3D12Device* pDevice,
	ID3D12GraphicsCommandList* pCommandList)
{
	int totalVertexCount = 0;
	for (auto& meshData : mMeshes) {
		totalVertexCount += meshData.Vertices.size();
	}

	std::vector<GeometryGenerator::Vertex> vertices(totalVertexCount);
	std::vector<std::uint16_t> indices;
	UINT k = 0;
	UINT meshId = 0;
	UINT indexOffset = 0, vertexOffset = 0;
	for (auto& meshData : mMeshes) {

		float x[2] = { 0 };
		float y[2] = { 0 };
		float z[2] = { 0 };

		if (meshData.Vertices.size() > 0) {
			x[0] = x[1] = meshData.Vertices[0].Position.x;
			y[0] = y[1] = meshData.Vertices[0].Position.y;
			z[0] = z[1] = meshData.Vertices[0].Position.z;
		}

		for (size_t i = 0; i < meshData.Vertices.size(); ++i, ++k)
		{
			vertices[k] = meshData.Vertices[i];

			x[0] = min(x[0], vertices[k].Position.x);
			x[1] = max(x[1], vertices[k].Position.x);
			y[0] = min(y[0], vertices[k].Position.y);
			y[1] = max(y[1], vertices[k].Position.y);
			z[0] = min(z[0], vertices[k].Position.z);
			z[1] = max(z[1], vertices[k].Position.z);
		}
		
		indices.insert(indices.end(), std::begin(meshData.GetIndices16()), std::end(meshData.GetIndices16()));

		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)meshData.Indices32.size();
		submesh.StartIndexLocation = indexOffset;
		submesh.BaseVertexLocation = vertexOffset;
		submesh.Bounds.Center = DirectX::XMFLOAT3((x[1] + x[0]) * 0.5f, (y[1] + y[0]) * 0.5f, (z[1] + z[0]) * 0.5f);
		submesh.Bounds.Extents = DirectX::XMFLOAT3((x[1] - x[0]) * 0.5f, (y[1] - y[0]) * 0.5f, (z[1] - z[0]) * 0.5f);
		mGeo.DrawArgs[std::to_string(meshId++)] = submesh;

		indexOffset += meshData.Indices32.size();
		vertexOffset += meshData.Vertices.size();
	}


	const UINT vbByteSize = (UINT)vertices.size() * sizeof(GeometryGenerator::Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	D3DCreateBlob(vbByteSize, &mGeo.VertexBufferCPU);
	CopyMemory(mGeo.VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	D3DCreateBlob(ibByteSize, &mGeo.IndexBufferCPU);
	CopyMemory(mGeo.IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mGeo.VertexBufferGPU = d3dUtil::CreateDefaultBuffer(pDevice,
		pCommandList, vertices.data(), vbByteSize, mGeo.VertexBufferUploader);

	mGeo.IndexBufferGPU = d3dUtil::CreateDefaultBuffer(pDevice,
		pCommandList, indices.data(), ibByteSize, mGeo.IndexBufferUploader);

	mGeo.VertexByteStride = sizeof(GeometryGenerator::Vertex);
	mGeo.VertexBufferByteSize = vbByteSize;
	mGeo.IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeo.IndexBufferByteSize = ibByteSize;
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
