#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "MyApp.h"
#include "RenderTexture.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 1;
const int gNumTex = 2;

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT4 color;
};

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();

	int NumFramesDirty = gNumFrameResources;

	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT3 EyePosWorld = { 0.0f,0.0f,0.0f };
};

struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
	{
		device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(CmdListAlloc.GetAddressOf())
		);

		PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
		ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	};

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	UINT64 Fence = 0;
};

class SrvRtvTexture :public RenderTexture
{
public:
	SrvRtvTexture(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		:RenderTexture(device, width, height, format, flag)
	{};

	virtual void BuildDescriptors()override;
};
void SrvRtvTexture::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	md3dDevice->CreateShaderResourceView(mRenderTex.Get(), &srvDesc, mhCpuSrv);


	md3dDevice->CreateRenderTargetView(mRenderTex.Get(), nullptr, mhCpuRtv);
}

class InvertColor :public MyApp
{
public:
	InvertColor(HINSTANCE hInstance);

	virtual bool Initialize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
private:
	void CreateRtvAndDsvDescriptorHeaps()override;

	std::vector<std::unique_ptr<SrvRtvTexture>> mTexs;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignaturePresent = nullptr;
	void BuildRootSignature();

	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC>mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC>mInputLayoutPresent;
	void BuildShadersAndInputLayout();

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	void BuildShapeGeometry();

	std::vector<std::unique_ptr<RenderItem>> mAllRenderitems;
	std::vector<RenderItem*> mOpaqueRenderitems;
	void BuildRenderItems();

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	int mCurrFrameResourceIndex = 0;
	FrameResource* mCurrFrameResource = nullptr;
	void BuildFrameResources();

	UINT mPassCbvOffset = 0;
	UINT mTexOffset = 0;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mPresentHeap = nullptr;
	PassConstants mMainPassCB;
	void BuildDescriptorHeaps();
	void BuildBuffers();

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	void BuildPSOs();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		InvertColor theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

InvertColor::InvertColor(HINSTANCE hInstance) :
	MyApp(hInstance)
{
}

bool InvertColor::Initialize()
{
	if (!MyApp::Initialize())return false;

	mTexs.push_back(std::make_unique<SrvRtvTexture>(
		md3dDevice.Get(),
		mClientWidth, mClientHeight,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));

	mTexs.push_back(std::make_unique<SrvRtvTexture>(
		md3dDevice.Get(),
		mClientWidth, mClientHeight,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));

	mCamera.SetPosition(XMFLOAT3(0, 5, -10));

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildRenderItems();
	BuildFrameResources();

	BuildDescriptorHeaps();
	BuildBuffers();

	BuildPSOs();

	mCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	return true;
}

void InvertColor::Update(const GameTimer& gt)
{
	MyApp::Update(gt);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	//todo:not understand
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	//:todo

	//Update Per Object CB
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRenderitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}

	//Update Main Pass Constant Buffer
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	mMainPassCB.EyePosWorld = mEyePos;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void InvertColor::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	cmdListAlloc->Reset();

	if (mIsWireframe) {
		mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get());
	}
	else {
		mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get());
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);


	// draw grid0
	{
		//mCommandList->ResourceBarrier(
		//	1, &CD3DX12_RESOURCE_BARRIER::Transition(
		//		CurrentBackBuffer(),
		//		D3D12_RESOURCE_STATE_PRESENT,
		//		D3D12_RESOURCE_STATE_RENDER_TARGET)
		//);
		mCommandList->ResourceBarrier(
			1, &CD3DX12_RESOURCE_BARRIER::Transition(
				mTexs[0]->Output(),
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_RENDER_TARGET)
		);

		mCommandList->ClearRenderTargetView(mTexs[0]->Rtv(), Colors::Black, 0, nullptr);
		mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &mTexs[0]->Rtv(), false, &DepthStencilView());

		ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
		auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

		std::vector<RenderItem*>ritemGrid0{ mOpaqueRenderitems[0] };
		DrawRenderItems(mCommandList.Get(), ritemGrid0);

		mCommandList->ResourceBarrier(
			1, &CD3DX12_RESOURCE_BARRIER::Transition(
				mTexs[0]->Output(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_COMMON)
		);

		//mCommandList->ResourceBarrier(
		//	1, &CD3DX12_RESOURCE_BARRIER::Transition(
		//		CurrentBackBuffer(),
		//		D3D12_RESOURCE_STATE_RENDER_TARGET,
		//		D3D12_RESOURCE_STATE_COPY_SOURCE)
		//);
		//mCommandList->ResourceBarrier(
		//	1, &CD3DX12_RESOURCE_BARRIER::Transition(
		//		mTexs[0]->Output(),
		//		D3D12_RESOURCE_STATE_COMMON,
		//		D3D12_RESOURCE_STATE_COPY_DEST)
		//);

		//mCommandList->CopyResource(mTexs[0]->Output(), CurrentBackBuffer());

		//mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		//	D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT));
	}

	// draw grid1
	{
		//mCommandList->ResourceBarrier(
		//	1, &CD3DX12_RESOURCE_BARRIER::Transition(
		//		CurrentBackBuffer(),
		//		D3D12_RESOURCE_STATE_PRESENT,
		//		D3D12_RESOURCE_STATE_RENDER_TARGET)
		//);
		mCommandList->ResourceBarrier(
			1, &CD3DX12_RESOURCE_BARRIER::Transition(
				mTexs[1]->Output(),
				D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_RENDER_TARGET)
		);

		mCommandList->ClearRenderTargetView(mTexs[1]->Rtv(), Colors::Black, 0, nullptr);
		mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &mTexs[1]->Rtv(), true, &DepthStencilView());

		ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
		auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

		std::vector<RenderItem*>ritemGrid1{ mOpaqueRenderitems[1] };
		DrawRenderItems(mCommandList.Get(), ritemGrid1);

		mCommandList->ResourceBarrier(
			1, &CD3DX12_RESOURCE_BARRIER::Transition(
				mTexs[1]->Output(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_COMMON)
		);

		//mCommandList->ResourceBarrier(
		//	1, &CD3DX12_RESOURCE_BARRIER::Transition(
		//		CurrentBackBuffer(),
		//		D3D12_RESOURCE_STATE_RENDER_TARGET,
		//		D3D12_RESOURCE_STATE_COPY_SOURCE)
		//);
		//mCommandList->ResourceBarrier(
		//	1, &CD3DX12_RESOURCE_BARRIER::Transition(
		//		mTexs[1]->Output(),
		//		D3D12_RESOURCE_STATE_COMMON,
		//		D3D12_RESOURCE_STATE_COPY_DEST)
		//);

		//mCommandList->CopyResource(mTexs[1]->Output(), CurrentBackBuffer());

		//mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		//	D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT));
	}

	// present
	{
		mCommandList->SetPipelineState(mPSOs["present"].Get());

		mCommandList->ResourceBarrier(
			1, &CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET)
		);

		mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);
		mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

		ID3D12DescriptorHeap* descriptorHeaps[] = { mPresentHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		mCommandList->SetGraphicsRootSignature(mRootSignaturePresent.Get());

		mCommandList->SetGraphicsRootDescriptorTable(0, mPresentHeap->GetGPUDescriptorHandleForHeapStart());

		mCommandList->IASetVertexBuffers(0, 1, &mGeometries["presentGeo"]->VertexBufferView());
		mCommandList->IASetIndexBuffer(&mGeometries["presentGeo"]->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		mCommandList->DrawIndexedInstanced(
			mGeometries["presentGeo"]->DrawArgs["present"].IndexCount,
			1, 0, 0, 0
		);


		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	}

	mCommandList->Close();

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void InvertColor::CreateRtvAndDsvDescriptorHeaps()
{
	// Add +gNumTex RTV for Texs
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + gNumTex * gNumFrameResources;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Common creation.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void InvertColor::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(
			std::make_unique<FrameResource>(
				md3dDevice.Get(),
				1, mAllRenderitems.size()
				)
		);
	};
}

void InvertColor::BuildRootSignature()
{
	{
		CD3DX12_DESCRIPTOR_RANGE cbvTable[2];
		cbvTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		cbvTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable[0]);
		slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			2, slotRootParameter, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(),
			errorBlob.GetAddressOf()
		);

		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignature.GetAddressOf())
		);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, mTexs.size(), 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[1];
		slotRootParameter[0].InitAsDescriptorTable(1, &texTable);

		auto staticSamplers = GetStaticSamplers();

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			sizeof(slotRootParameter) / sizeof(CD3DX12_ROOT_PARAMETER),
			slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(),
			errorBlob.GetAddressOf()
		);

		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignaturePresent.GetAddressOf())
		);
	}
}

void InvertColor::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\InvertColor\\shader.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\InvertColor\\shader.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["presentVS"] = d3dUtil::CompileShader(L"Shaders\\InvertColor\\present.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["presentPS"] = d3dUtil::CompileShader(L"Shaders\\InvertColor\\present.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0} ,
		{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,sizeof(float) * 3,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};

	mInputLayoutPresent = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0} ,
		{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,sizeof(float) * 3,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};
}

void InvertColor::BuildShapeGeometry()
{
	{
		GeometryGenerator geoGen;
		GeometryGenerator::MeshData grid0 = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
		GeometryGenerator::MeshData grid1 = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);

		UINT grid0VertexOffset = 0;
		UINT grid0IndexOffset = 0;

		UINT grid1VertexOffset = grid0.Vertices.size();
		UINT grid1IndexOffset = grid0.Indices32.size();

		SubmeshGeometry grid0Submesh;
		grid0Submesh.IndexCount = (UINT)grid0.Indices32.size();
		grid0Submesh.StartIndexLocation = grid0IndexOffset;
		grid0Submesh.BaseVertexLocation = grid0VertexOffset;

		SubmeshGeometry grid1Submesh;
		grid1Submesh.IndexCount = (UINT)grid1.Indices32.size();
		grid1Submesh.StartIndexLocation = grid1IndexOffset;
		grid1Submesh.BaseVertexLocation = grid1VertexOffset;

		auto totalVertexCount = grid0.Vertices.size() + grid1.Vertices.size();

		std::vector<Vertex> vertices(totalVertexCount);

		UINT k = 0;

		for (size_t i = 0; i < grid0.Vertices.size(); ++i, ++k)
		{
			vertices[k].pos = grid0.Vertices[i].Position;
			vertices[k].color = XMFLOAT4(DirectX::Colors::ForestGreen);
		}
		for (size_t i = 0; i < grid1.Vertices.size(); ++i, ++k)
		{
			vertices[k].pos = grid1.Vertices[i].Position;
			vertices[k].color = XMFLOAT4(0.3, 0.2, 0.8, 1);
		}

		std::vector<std::uint16_t> indices;
		indices.insert(indices.end(), std::begin(grid0.GetIndices16()), std::end(grid0.GetIndices16()));
		indices.insert(indices.end(), std::begin(grid1.GetIndices16()), std::end(grid1.GetIndices16()));

		const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto geo = std::make_unique<MeshGeometry>();
		geo->Name = "shapeGeo";

		D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU);
		CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

		D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU);
		CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

		geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

		geo->VertexByteStride = sizeof(Vertex);
		geo->VertexBufferByteSize = vbByteSize;
		geo->IndexFormat = DXGI_FORMAT_R16_UINT;
		geo->IndexBufferByteSize = ibByteSize;

		geo->DrawArgs["grid0"] = grid0Submesh;
		geo->DrawArgs["grid1"] = grid1Submesh;

		mGeometries[geo->Name] = std::move(geo);
	}

	// build present geo
	{
		std::array<Vertex, 4> vertices =
		{
			Vertex({ XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT4(0,0,0,0)}),//left top
			Vertex({ XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT4(1,0,0,0)}),//right top
			Vertex({ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT4(0,1,0,0)}),//left bottom
			Vertex({ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT4(1,1,0,0)}),//right bottom
		};
		std::array<std::uint16_t, 6> indices =
		{
			0,1,2,
			1,3,2
		};

		const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto presentGeo = std::make_unique<MeshGeometry>();
		presentGeo->Name = "presentGeo";
		ThrowIfFailed(D3DCreateBlob(vbByteSize, &presentGeo->VertexBufferCPU));
		CopyMemory(presentGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
		ThrowIfFailed(D3DCreateBlob(ibByteSize, &presentGeo->IndexBufferCPU));
		CopyMemory(presentGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		presentGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), vertices.data(), vbByteSize, presentGeo->VertexBufferUploader);
		presentGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), indices.data(), ibByteSize, presentGeo->IndexBufferUploader);

		presentGeo->VertexByteStride = sizeof(Vertex);
		presentGeo->VertexBufferByteSize = vbByteSize;
		presentGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
		presentGeo->IndexBufferByteSize = ibByteSize;

		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)indices.size();
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		presentGeo->DrawArgs["present"] = submesh;
		mGeometries[presentGeo->Name] = std::move(presentGeo);
	}
}

void InvertColor::BuildRenderItems()
{
	UINT objCBIndex = 0;

	auto grid0Ritem = std::make_unique<RenderItem>();
	grid0Ritem->World = MathHelper::Identity4x4();
	grid0Ritem->ObjCBIndex = objCBIndex++;
	grid0Ritem->Geo = mGeometries["shapeGeo"].get();
	grid0Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	grid0Ritem->IndexCount = grid0Ritem->Geo->DrawArgs["grid0"].IndexCount;
	grid0Ritem->StartIndexLocation = grid0Ritem->Geo->DrawArgs["grid0"].StartIndexLocation;
	grid0Ritem->BaseVertexLocation = grid0Ritem->Geo->DrawArgs["grid0"].BaseVertexLocation;
	mAllRenderitems.push_back(std::move(grid0Ritem));

	auto grid1Ritem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&grid1Ritem->World, XMMatrixTranslation(-5.0f, 2.0f, 0));
	grid1Ritem->ObjCBIndex = objCBIndex++;
	grid1Ritem->Geo = mGeometries["shapeGeo"].get();
	grid1Ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	grid1Ritem->IndexCount = grid1Ritem->Geo->DrawArgs["grid1"].IndexCount;
	grid1Ritem->StartIndexLocation = grid1Ritem->Geo->DrawArgs["grid1"].StartIndexLocation;
	grid1Ritem->BaseVertexLocation = grid1Ritem->Geo->DrawArgs["grid1"].BaseVertexLocation;
	mAllRenderitems.push_back(std::move(grid1Ritem));

	// All the render items are opaque.
	for (auto& e : mAllRenderitems) mOpaqueRenderitems.push_back(e.get());
}

void InvertColor::BuildDescriptorHeaps()
{
	{
		UINT objCount = (UINT)mOpaqueRenderitems.size();
		UINT frameCount = gNumFrameResources;

		UINT numDescriptors = objCount * gNumFrameResources + frameCount;

		mPassCbvOffset = objCount * gNumFrameResources;

		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
		cbvHeapDesc.NumDescriptors = numDescriptors;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.NodeMask = 0;
		md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap));
	}

	{
		mTexOffset = 0;

		D3D12_DESCRIPTOR_HEAP_DESC presentHeapDesc;
		presentHeapDesc.NumDescriptors = mTexs.size() * gNumFrameResources;
		presentHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		presentHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		presentHeapDesc.NodeMask = 0;
		ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&presentHeapDesc, IID_PPV_ARGS(&mPresentHeap)));
	}
}

void InvertColor::BuildBuffers()
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT objCount = mOpaqueRenderitems.size();

	for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();

		for (int objectIndex = 0; objectIndex < objCount; objectIndex++) {
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();
			cbAddress += objectIndex * objCBByteSize;

			int heapIndex = objCount * frameIndex + objectIndex;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;
			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();

		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();
		//cbAddress += frameIndex; 
		//todo: why not add address?

		int heapIndex = mPassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;
		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}

	// present use buffer
	{
		{
			for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {

				int shadowMapTexIndex = mTexOffset + frameIndex;

				auto srvCpuStart = mPresentHeap->GetCPUDescriptorHandleForHeapStart();
				auto srvGpuStart = mPresentHeap->GetGPUDescriptorHandleForHeapStart();
				auto rtvCpuStart = mRtvHeap->GetCPUDescriptorHandleForHeapStart();

				for (int i = 0; i < mTexs.size(); i++) {
					mTexs[i]->RenderTexture::BuildDescriptors(
						CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mTexOffset + i + frameIndex, mCbvSrvUavDescriptorSize),
						CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mTexOffset + i + frameIndex, mCbvSrvUavDescriptorSize),
						CD3DX12_CPU_DESCRIPTOR_HANDLE(),
						CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, SwapChainBufferCount + i + frameIndex, mRtvDescriptorSize));
				}
			}
		}

	}
}

void InvertColor::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mTexs[0]->Format();
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"]));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"]));

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC presentPsoDesc = opaquePsoDesc;
		presentPsoDesc.InputLayout = { mInputLayoutPresent.data(), (UINT)mInputLayoutPresent.size() };
		presentPsoDesc.pRootSignature = mRootSignaturePresent.Get();
		presentPsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["presentVS"]->GetBufferPointer()),
			mShaders["presentVS"]->GetBufferSize()
		};
		presentPsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["presentPS"]->GetBufferPointer()),
			mShaders["presentPS"]->GetBufferSize()
		};
		presentPsoDesc.NumRenderTargets = 1;
		presentPsoDesc.RTVFormats[0] = mBackBufferFormat;

		md3dDevice->CreateGraphicsPipelineState(&presentPsoDesc, IID_PPV_ARGS(&mPSOs["present"]));
	}
}

void InvertColor::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRenderitems.size() + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}
