#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/GeometryGenerator.h"
#include "MyApp.h"
#include "Model.h"
#include "Toolkit.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrame = 1;
const int gNumObjects = 8 * 4 * 4 * 4;

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT4 color;
};

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();

	int NumFramesDirty = gNumFrame;

	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;
	BoundingBox BoundingBox;

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
	XMFLOAT4 boxCenter;
	XMFLOAT4 boxLen;
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
	};

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<CullPassInfo>> CullPassCB = nullptr;

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

	const UINT mComputeThreadBlockSize = 128;
	XMFLOAT4X4 mProjCull;

	enum class RootParametersCull : int
	{
		PassCbv,
		ObjectInfoSrv,
		CommandsSrv,
		OutputCommandsUav,
		Size
	};
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignatureCull = nullptr;
	void BuildRootSignature();

	struct IndirectCommand
	{
		D3D12_GPU_VIRTUAL_ADDRESS cbvPerObj;
		D3D12_GPU_VIRTUAL_ADDRESS cbvPerPass;
		D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;
		UINT pad[3];
	};
	enum class GraphicsRootParameters : int
	{
		CbvPerObj = 0,
		CbvPerPass,
		Size
	};
	ComPtr<ID3D12CommandSignature> mCommandSignature = nullptr;
	void BuildCommandSignature();

	enum class HeapCullOffsets : int
	{
		PassCbOffset = 0,
		ObjectsSrvOffset = PassCbOffset + 1,
		CommandsOffset = ObjectsSrvOffset + 1,
		ProcessedCommandsOffset = CommandsOffset + 1,
		Size = ProcessedCommandsOffset + 1
	};
	ComPtr<ID3D12DescriptorHeap> mHeapDraw = nullptr;
	ComPtr<ID3D12DescriptorHeap> mHeapCull = nullptr;
	PassConstants mMainPassCB;
	void BuildDescriptorHeaps();

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

	UINT mCommandBufferCounterOffset = AlignForUavCounter(gNumObjects * sizeof(IndirectCommand) * gNumFrame);
	std::unique_ptr<UploadBuffer<CullObjectInfo>> mCullObjectBuffer = nullptr;
	std::unique_ptr<UploadBuffer<IndirectCommand>> mCommandsBuffer = nullptr;
	ComPtr<ID3D12Resource> mProcessedCommandBuffers[gNumFrame];
	ComPtr<ID3D12Resource> mProcessedCommandBufferCounterReset;
	void BuildBuffers();

	void BuildCommands();
	void BuildObjectsCullInfo();

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	void BuildPSOs();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	static inline UINT AlignForUavCounter(UINT bufferSize)
	{
		const UINT alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
		return (bufferSize + (alignment - 1)) & ~(alignment - 1);
	}
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
	mCamera.SetLens(20.0f, (float)mClientWidth / mClientHeight, 1, 3000);
	mProjCull = mCamera.GetProj4x4f();
	mCamera.SetLens(90.0f, (float)mClientWidth / mClientHeight, 1, 3000);

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildShadersAndInputLayout();
	LoadModels();
	BuildRenderItems();
	BuildFrameResources();

	BuildRootSignature();
	BuildCommandSignature();
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

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrame;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	//Update Main Pass Constant Buffer
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	mMainPassCB.EyePosWorld = mEyePos;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);

	// update cull_cs pass CB
	{
		auto currPassCB = mCurrFrameResource->CullPassCB.get();
		CullPassInfo passInfo;
		XMStoreFloat4x4(&passInfo.View, XMMatrixTranspose(view));
		XMStoreFloat4x4(&passInfo.Proj, XMMatrixTranspose(XMLoadFloat4x4(&mProjCull)));
		passInfo.CommandCount = gNumObjects;
		currPassCB->CopyData(0, passInfo);
	}
}

void ComputeCull::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	cmdListAlloc->Reset();

	// execute culling cs
	{
		mCommandList->Reset(cmdListAlloc.Get(), mPSOs["cull"].Get());

		mCommandList->SetComputeRootSignature(mRootSignatureCull.Get());

		ID3D12DescriptorHeap* ppHeaps[] = { mHeapCull.Get() };
		mCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		int passCbvIndex = (UINT)HeapCullOffsets::PassCbOffset + (mCurrFrameResourceIndex * (UINT)HeapCullOffsets::Size);
		auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapCull->GetGPUDescriptorHandleForHeapStart());
		passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetComputeRootDescriptorTable((UINT)RootParametersCull::PassCbv, passCbvHandle);

		auto objInfoBuffer = mCullObjectBuffer->Resource();
		mCommandList->SetComputeRootShaderResourceView((UINT)RootParametersCull::ObjectInfoSrv, objInfoBuffer->GetGPUVirtualAddress());

		auto commandsBuffer = mCommandsBuffer->Resource();
		mCommandList->SetComputeRootShaderResourceView((UINT)RootParametersCull::CommandsSrv, commandsBuffer->GetGPUVirtualAddress());

		int outCommandsUavIndex = (UINT)HeapCullOffsets::ProcessedCommandsOffset + (mCurrFrameResourceIndex * (UINT)HeapCullOffsets::Size);
		auto outCommandsUavHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapCull->GetGPUDescriptorHandleForHeapStart());
		outCommandsUavHandle.Offset(outCommandsUavIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetComputeRootDescriptorTable((UINT)RootParametersCull::OutputCommandsUav, outCommandsUavHandle);

		mCommandList->CopyBufferRegion(
			mProcessedCommandBuffers[mCurrFrameResourceIndex].Get(),
			mCommandBufferCounterOffset, mProcessedCommandBufferCounterReset.Get(), 0, sizeof(UINT));

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mProcessedCommandBuffers[mCurrFrameResourceIndex].Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		mCommandList->Dispatch(static_cast<UINT>(ceil((float)gNumObjects / float(mComputeThreadBlockSize))), 1, 1);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mProcessedCommandBuffers[mCurrFrameResourceIndex].Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST));
	}

	// draw command
	{
		mCommandList->SetPipelineState(mPSOs["opaque"].Get());

		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		ID3D12DescriptorHeap* ppHeaps[] = { mHeapDraw.Get() };
		mCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mProcessedCommandBuffers[mCurrFrameResourceIndex].Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), FALSE, &DepthStencilView());


		// For each render item... 
		for (size_t i = 0; i < mOpaqueRenderitems.size(); ++i)
		{
			auto ri = mOpaqueRenderitems[i];

			mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
			mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
			mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);
		}

		mCommandList->ExecuteIndirect(
			mCommandSignature.Get(),
			gNumObjects,
			mProcessedCommandBuffers[mCurrFrameResourceIndex].Get(),
			0,
			mProcessedCommandBuffers[mCurrFrameResourceIndex].Get(),
			mCommandBufferCounterOffset);

		//mCommandList->ExecuteIndirect(
		//	mCommandSignature.Get(),
		//	gNumObjects,
		//	mCommandsBuffer->Resource(),
		//	gNumObjects * sizeof(IndirectCommand) * mCurrFrameResourceIndex,
		//	nullptr,
		//	0);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mProcessedCommandBuffers[mCurrFrameResourceIndex].Get(),
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
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

void ComputeCull::BuildFrameResources()
{
	for (int i = 0; i < gNumFrame; i++) {
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
		CD3DX12_ROOT_PARAMETER slotRootParameter[(UINT)GraphicsRootParameters::Size];
		slotRootParameter[(UINT)GraphicsRootParameters::CbvPerObj].InitAsConstantBufferView(0);
		slotRootParameter[(UINT)GraphicsRootParameters::CbvPerPass].InitAsConstantBufferView(1);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			(UINT)GraphicsRootParameters::Size, slotRootParameter, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()
		);

		ThrowIfFailed(md3dDevice->CreateRootSignature(
			0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignature.GetAddressOf())
		));
	}

	{
		CD3DX12_DESCRIPTOR_RANGE passCbvTable;
		passCbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE uavTable;
		uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER computeRootParameters[(UINT)RootParametersCull::Size];
		computeRootParameters[(UINT)RootParametersCull::PassCbv].InitAsDescriptorTable(1, &passCbvTable);
		computeRootParameters[(UINT)RootParametersCull::ObjectInfoSrv].InitAsShaderResourceView(0);
		computeRootParameters[(UINT)RootParametersCull::CommandsSrv].InitAsShaderResourceView(1);
		computeRootParameters[(UINT)RootParametersCull::OutputCommandsUav].InitAsDescriptorTable(1, &uavTable);

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

		ThrowIfFailed(md3dDevice->CreateRootSignature(
			0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignatureCull.GetAddressOf())
		));
	}
}

void ComputeCull::BuildCommandSignature()
{
	{
		D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[3] = {};
		argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
		argumentDescs[0].ConstantBufferView.RootParameterIndex = (UINT)GraphicsRootParameters::CbvPerObj;
		argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
		argumentDescs[1].ConstantBufferView.RootParameterIndex = (UINT)GraphicsRootParameters::CbvPerPass;
		argumentDescs[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
		commandSignatureDesc.pArgumentDescs = argumentDescs;
		commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
		commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

		ThrowIfFailed(md3dDevice->CreateCommandSignature(&commandSignatureDesc, mRootSignature.Get(), IID_PPV_ARGS(&mCommandSignature)));
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
					renderItem->BoundingBox = drawArg.second.Bounds;
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
		UINT frameCount = gNumFrame;

		UINT numDescriptors = objCount * gNumFrame + frameCount;

		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
		cbvHeapDesc.NumDescriptors = numDescriptors;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.NodeMask = 0;
		md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mHeapDraw));
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
		cbvHeapDesc.NumDescriptors = (UINT)HeapCullOffsets::Size * gNumFrame;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.NodeMask = 0;
		md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mHeapCull));
	}
}

void ComputeCull::BuildBuffers()
{
	{
		//Store Per Object CB
		for (int i = 0; i < mFrameResources.size(); i++) {
			auto currObjectCB = mFrameResources[i]->ObjectCB.get();
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
		}
	}

	// for culling cs
	{
		UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(CullPassInfo));

		for (int frameIndex = 0; frameIndex < gNumFrame; frameIndex++) {
			auto passCB = mFrameResources[frameIndex]->CullPassCB->Resource();

			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

			int heapIndex = (UINT)HeapCullOffsets::PassCbOffset + (frameIndex * (UINT)HeapCullOffsets::Size);
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeapCull->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = passCBByteSize;
			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}

		{
			mCullObjectBuffer = std::make_unique<UploadBuffer<CullObjectInfo>>(md3dDevice.Get(), gNumObjects * gNumFrame, false);

			for (int frameIndex = 0; frameIndex < gNumFrame; frameIndex++) {

				int heapIndex = (UINT)HeapCullOffsets::ObjectsSrvOffset + (frameIndex * (UINT)HeapCullOffsets::Size);
				auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeapCull->GetCPUDescriptorHandleForHeapStart());
				handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Buffer.NumElements = gNumObjects;
				srvDesc.Buffer.StructureByteStride = sizeof(CullObjectInfo);
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				md3dDevice->CreateShaderResourceView(mCullObjectBuffer->Resource(), &srvDesc, handle);
			}

			BuildObjectsCullInfo();
		}

		{
			mCommandsBuffer = std::make_unique<UploadBuffer<IndirectCommand>>(md3dDevice.Get(), gNumObjects * gNumFrame, false);

			for (int frameIndex = 0; frameIndex < gNumFrame; frameIndex++) {

				int heapIndex = (UINT)HeapCullOffsets::CommandsOffset + (frameIndex * (UINT)HeapCullOffsets::Size);
				auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeapCull->GetCPUDescriptorHandleForHeapStart());
				handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Buffer.NumElements = gNumObjects;
				srvDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				md3dDevice->CreateShaderResourceView(mCommandsBuffer->Resource(), &srvDesc, handle);
			}

			BuildCommands();
		}


		// gen commands buffer

		for (UINT frame = 0; frame < gNumFrame; frame++)
		{
			int heapIndex = (UINT)HeapCullOffsets::ProcessedCommandsOffset + (frame * (UINT)HeapCullOffsets::Size);
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeapCull->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			// Allocate a buffer large enough to hold all of the indirect commands
			// for a single frame as well as a UAV counter.
			CD3DX12_RESOURCE_DESC commandBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
				mCommandBufferCounterOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			ThrowIfFailed(md3dDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&commandBufferDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&mProcessedCommandBuffers[frame])));

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = gNumObjects;
			uavDesc.Buffer.StructureByteStride = sizeof(IndirectCommand);
			uavDesc.Buffer.CounterOffsetInBytes = mCommandBufferCounterOffset;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

			md3dDevice->CreateUnorderedAccessView(
				mProcessedCommandBuffers[frame].Get(),
				mProcessedCommandBuffers[frame].Get(),
				&uavDesc,
				handle);
		}

		// Allocate a buffer that can be used to reset the UAV counters and initialize it to 0.
		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mProcessedCommandBufferCounterReset)));

		UINT8* pMappedCounterReset = nullptr;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(mProcessedCommandBufferCounterReset->Map(0, &readRange, reinterpret_cast<void**>(&pMappedCounterReset)));
		ZeroMemory(pMappedCounterReset, sizeof(UINT));
		mProcessedCommandBufferCounterReset->Unmap(0, nullptr);
	}
}

void ComputeCull::BuildCommands()
{
	std::vector<IndirectCommand> commands;
	commands.resize(gNumObjects * gNumFrame);
	
	UINT commandIndex = 0;
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	for (UINT frame = 0; frame < gNumFrame; frame++)
	{
		for (UINT i = 0; i < min(gNumObjects, mOpaqueRenderitems.size()); i++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbvPerObjGpuAddress =
				mFrameResources[frame]->ObjectCB->Resource()->GetGPUVirtualAddress() + i * objCBByteSize;
			D3D12_GPU_VIRTUAL_ADDRESS cbvPerPassGpuAddress =
				mFrameResources[frame]->PassCB->Resource()->GetGPUVirtualAddress() + frame * passCBByteSize;

			auto ri = mOpaqueRenderitems[i];

			commands[commandIndex].cbvPerObj = cbvPerObjGpuAddress;
			commands[commandIndex].cbvPerPass = cbvPerPassGpuAddress;
			commands[commandIndex].drawArguments.BaseVertexLocation = ri->BaseVertexLocation;
			commands[commandIndex].drawArguments.IndexCountPerInstance = ri->IndexCount;
			commands[commandIndex].drawArguments.InstanceCount = 1;
			commands[commandIndex].drawArguments.StartIndexLocation = ri->StartIndexLocation;
			commands[commandIndex].drawArguments.StartInstanceLocation = 0;

			commandIndex++;
		}
	}

	// Copy data to the intermediate upload heap and then schedule a copy
	// from the upload heap to the command buffer.
	const UINT commandBufferSize = gNumObjects * sizeof(IndirectCommand) * gNumFrame + (sizeof(UINT) * gNumFrame);

	D3D12_SUBRESOURCE_DATA commandData = {};
	commandData.pData = reinterpret_cast<UINT8*>(&commands[0]);
	commandData.RowPitch = commandBufferSize;
	commandData.SlicePitch = commandData.RowPitch;

	for (int i = 0; i < commands.size(); i++) {
		mCommandsBuffer->CopyData(i, commands[i]);
	}
}

void ComputeCull::BuildObjectsCullInfo()
{
	for (int i = 0; i < min(mOpaqueRenderitems.size(), gNumObjects); i++) {
		CullObjectInfo objInfo;
		objInfo.World = mOpaqueRenderitems[i]->World;

		auto float3 = mOpaqueRenderitems[i]->BoundingBox.Center;
		objInfo.boxCenter = XMFLOAT4(float3.x, float3.y, float3.z, 1.0f);

		float3 = mOpaqueRenderitems[i]->BoundingBox.Extents;
		objInfo.boxLen = XMFLOAT4(float3.x, float3.y, float3.z, 1.0f);

		mCullObjectBuffer->CopyData(i, objInfo);
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
		D3D12_COMPUTE_PIPELINE_STATE_DESC cullPsoDesc = {};
		cullPsoDesc.pRootSignature = mRootSignatureCull.Get();
		cullPsoDesc.CS = {
			reinterpret_cast<BYTE*>(mShaders["cullCS"]->GetBufferPointer()),
			mShaders["cullCS"]->GetBufferSize()
		};
		cullPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		ThrowIfFailed(md3dDevice->CreateComputePipelineState(&cullPsoDesc, IID_PPV_ARGS(&mPSOs["cull"])));
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

		cmdList->SetGraphicsRootConstantBufferView(
			(UINT)GraphicsRootParameters::CbvPerObj,
			mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress() + 
			(ri->ObjCBIndex * objCBByteSize));

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}
