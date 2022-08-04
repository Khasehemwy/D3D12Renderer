#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/GeometryGenerator.h"
#include "MyApp.h"
#include "Model.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;
const int gNumObjects = 8 * 200 * 20;

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

struct CullPassInfo
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	float CommandCount;
};

struct CullObjectInfo
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT3 boxCenter;
	float boxLen;
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
		CullPassCB = std::make_unique<UploadBuffer<CullPassInfo>>(device, passCount, true);
		ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
		CullObjectCB = std::make_unique<UploadBuffer<CullObjectInfo>>(device, objectCount, true);
	};

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<CullPassInfo>> CullPassCB = nullptr;
	std::unique_ptr<UploadBuffer<CullObjectInfo>> CullObjectCB = nullptr;

	UINT64 Fence = 0;
};

class ComputeCull :public MyApp
{
public:
	ComputeCull(HINSTANCE hInstance);

	virtual bool Initialize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
private:

	enum RootParametersCull
	{
		SrvUavTable,
		RootConstants,            
		Size
	};
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignatureCull = nullptr;
	void BuildRootSignature();

	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC>mInputLayout;
	void BuildShadersAndInputLayout();

	std::unordered_map<std::string, std::unique_ptr<Model>> mModels;
	void LoadModels();

	std::vector<std::unique_ptr<RenderItem>> mAllRenderitems;
	std::vector<RenderItem*> mOpaqueRenderitems;
	void BuildRenderItems();

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	int mCurrFrameResourceIndex = 0;
	FrameResource* mCurrFrameResource = nullptr;
	void BuildFrameResources();

	UINT mPassCbvOffset = 0;
	enum class HeapCullOffsets : int
	{
		CbvSrvOffset = 0,                                                   
		CommandsOffset = CbvSrvOffset + 1,                                    
		ProcessedCommandsOffset = CommandsOffset + 1,                        
		Size = ProcessedCommandsOffset + 1
	};
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mHeapCull = nullptr;
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
		ComputeCull theApp(hInstance);
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

ComputeCull::ComputeCull(HINSTANCE hInstance) :
	MyApp(hInstance)
{
}

bool ComputeCull::Initialize()
{
	if (!MyApp::Initialize())return false;

	mCamera.SetPosition(XMFLOAT3(0, 5, -50));

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildRootSignature();
	BuildShadersAndInputLayout();
	LoadModels();
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

void ComputeCull::Update(const GameTimer& gt)
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

void ComputeCull::Draw(const GameTimer& gt)
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

	mCommandList->ResourceBarrier(
		1, &CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(mCommandList.Get(), mOpaqueRenderitems);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	mCommandList->Close();

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void ComputeCull::BuildFrameResources()
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

void ComputeCull::BuildRootSignature()
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
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()
		);

		md3dDevice->CreateRootSignature(
			0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignature.GetAddressOf())
		);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER computeRootParameters[(UINT)RootParametersCull::Size];
		computeRootParameters[(UINT)RootParametersCull::SrvUavTable].InitAsDescriptorTable(2, ranges);
		computeRootParameters[(UINT)RootParametersCull::RootConstants].InitAsConstants(3, 0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			(UINT)RootParametersCull::Size, computeRootParameters, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()
		);

		md3dDevice->CreateRootSignature(
			0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignatureCull.GetAddressOf())
		);
	}
}

void ComputeCull::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"..\\Shaders\\ComputeCulling\\shader.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"..\\Shaders\\ComputeCulling\\shader.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["cullCS"] = d3dUtil::CompileShader(L"..\\Shaders\\ComputeCulling\\cull_cs.hlsl", nullptr, "CS", "cs_5_1");

	mInputLayout = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0} ,
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,sizeof(XMFLOAT3),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TANGENTU",0,DXGI_FORMAT_R32G32B32_FLOAT,0,sizeof(XMFLOAT3) * 2,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXC",0,DXGI_FORMAT_R32G32_FLOAT,0,sizeof(XMFLOAT3) * 3,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};
}

void ComputeCull::LoadModels()
{
	std::unique_ptr<Model> pacman = std::make_unique<Model>("pacman", "../resources/pacman/Pacman.stl", md3dDevice.Get(), mCommandList.Get());

	mModels[pacman->Geo()->Name] = std::move(pacman);
}

void ComputeCull::BuildRenderItems()
{
	int objCBIndex = 0;
	float r = 0;
	int len = std::cbrt(gNumObjects / 8);
	int step = 200;
	for (int x = -len; x < len; x++) {
		for (int y = -len; y < len; y++) {
			for (int z = -len; z < len; z++) {
				XMMATRIX worldMatrix = XMMatrixIdentity();
				worldMatrix = worldMatrix * XMMatrixTranslation(x * step, y * step, z * step);
				worldMatrix *= XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), 0.0f, 0.0f);
				XMFLOAT3 color(r, 0, 0); r += 1.0 / gNumObjects;

				for (auto& drawArg : mModels["pacman"]->Geo()->DrawArgs) {
					auto renderItem = std::make_unique<RenderItem>();
					XMStoreFloat4x4(&renderItem->World, worldMatrix);
					renderItem->ObjCBIndex = objCBIndex++;
					renderItem->Geo = const_cast<MeshGeometry*>(mModels["pacman"]->Geo());
					renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
					renderItem->IndexCount = drawArg.second.IndexCount;
					renderItem->StartIndexLocation = drawArg.second.StartIndexLocation;
					renderItem->BaseVertexLocation = drawArg.second.BaseVertexLocation;
					mAllRenderitems.push_back(std::move(renderItem));
				}
			}
		}
	}


	// All the render items are opaque.
	for (auto& e : mAllRenderitems) mOpaqueRenderitems.push_back(e.get());
}

void ComputeCull::BuildDescriptorHeaps()
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
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
		cbvHeapDesc.NumDescriptors = (UINT)HeapCullOffsets::Size;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.NodeMask = 0;
		md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mHeapCull));
	}
}

void ComputeCull::BuildBuffers()
{
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

			int heapIndex = mPassCbvOffset + frameIndex;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = passCBByteSize;
			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	{

	}
}

void ComputeCull::BuildPSOs()
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
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"]));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"]));

	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC blurPsoDesc;
		ZeroMemory(&blurPsoDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));
		blurPsoDesc.pRootSignature = mRootSignatureCull.Get();
		blurPsoDesc.CS = {
			reinterpret_cast<BYTE*>(mShaders["cullCS"]->GetBufferPointer()),
			mShaders["cullCS"]->GetBufferSize()
		};
		blurPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		md3dDevice->CreateComputePipelineState(&blurPsoDesc, IID_PPV_ARGS(&mPSOs["cull"]));
	}
}

void ComputeCull::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
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
