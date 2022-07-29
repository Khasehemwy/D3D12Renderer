#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "MyApp.h"
#include "Model.h"
#include "RenderTexture.h"
#include "DebugViewer.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 1;
const int gNumGBuffer = 2;
const int gNumRandVec = 14;

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
};

struct SsaoPassConstants
{
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ProjTex = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4 OffsetVectors[gNumRandVec];
	float OcclusionRadius;
	float SurfaceEpsilon;
	float OcclusionFadeStart;
	float OcclusionFadeEnd;
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
		SsaoPassCB = std::make_unique<UploadBuffer<SsaoPassConstants>>(device, passCount, true);
		ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	};

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<SsaoPassConstants>> SsaoPassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	UINT64 Fence = 0;
};

class NormalBuffer :public RenderTexture
{
public:
	NormalBuffer(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		:RenderTexture(device, width, height, format, flag)
	{};

	virtual void BuildDescriptors()override
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		md3dDevice->CreateShaderResourceView(mRenderTex.Get(), &srvDesc, mhCpuSrv);


		md3dDevice->CreateRenderTargetView(mRenderTex.Get(), nullptr, mhCpuRtv);
	}
};

class ZBuffer :public RenderTexture
{
public:
	ZBuffer(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R24G8_TYPELESS,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		:RenderTexture(device, width, height, format, flag)
	{};

	virtual void BuildDescriptors()override
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;
		md3dDevice->CreateShaderResourceView(mRenderTex.Get(), &srvDesc, mhCpuSrv);

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2D.MipSlice = 0;
		md3dDevice->CreateDepthStencilView(mRenderTex.Get(), &dsvDesc, mhCpuDsv);
	}
};

class RandomVectorMap : public RenderTexture
{
public:
	RandomVectorMap(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_NONE)
		:RenderTexture(device, width, height, format, flag)
	{};

	virtual void BuildDescriptors()override
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;
		md3dDevice->CreateShaderResourceView(mRenderTex.Get(), &srvDesc, mhCpuSrv);
	}
};

class SsaoMap :public RenderTexture
{
public:
	SsaoMap(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R16_FLOAT,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		:RenderTexture(device, width, height, format, flag)
	{};

	virtual void BuildDescriptors()override
	{
		//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		//srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
		//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		//srvDesc.Texture2D.MostDetailedMip = 0;
		//srvDesc.Texture2D.MipLevels = 1;
		//md3dDevice->CreateShaderResourceView(mRenderTex.Get(), &srvDesc, mhCpuSrv);


		md3dDevice->CreateRenderTargetView(mRenderTex.Get(), nullptr, mhCpuRtv);
	}
};

class SSAO :public MyApp
{
public:
	SSAO(HINSTANCE hInstance);

	virtual bool Initialize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
private:
	std::unique_ptr<DebugViewer> mDebugViewerNormal;
	std::unique_ptr<DebugViewer> mDebugViewerZ;
	std::unique_ptr<DebugViewer> mDebugViewerSsaoMap;
	std::unique_ptr<DebugViewer> mDebugViewerRandomVec;

	virtual void CreateRtvAndDsvDescriptorHeaps()override;

	std::array<std::unique_ptr<RenderTexture>, gNumGBuffer>mGbuffer;
	// gbuffer[0]: normal
	// gbuffer[1]: z
	std::unique_ptr<SsaoMap> mSsaoMap;

	ComPtr<ID3D12RootSignature> mRootSignatureGbuffer = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignatureSsaoMap = nullptr;
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
	UINT mSsaoPassCbvOffset = 0;
	UINT mNormalBufferSrvOffset = 0;
	UINT mNormalBufferRtvOffset = 0;
	UINT mZBufferSrvOffset = 0;
	UINT mZBufferDsvOffset = 0;
	UINT mSsaoMapSrvOffset = 0;
	UINT mSsaoMapRtvOffset = 0;
	UINT mRandomVectorMapSrvOffset = 0;
	ComPtr<ID3D12DescriptorHeap> mHeapGbuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> mHeapSsaoMap = nullptr;
	PassConstants mMainPassCB;
	SsaoPassConstants mSsaoPassCB;
	void BuildDescriptorHeaps();
	void BuildBuffers();

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	void BuildPSOs();

	XMFLOAT4 mOffsetVectors[14];
	void BuildOffsetVectors();

	std::unique_ptr<RandomVectorMap> mRandomVectorMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer;
	void GenRandomVectorMap();

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
		SSAO theApp(hInstance);
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

SSAO::SSAO(HINSTANCE hInstance) :
	MyApp(hInstance)
{
}

void SSAO::CreateRtvAndDsvDescriptorHeaps()
{
	// Add +gNumFrameResources RTV for normal-buffer.
	// Add +gNumFrameResources RTV for ssaoMap.
	mNormalBufferRtvOffset = SwapChainBufferCount;
	mSsaoMapRtvOffset = SwapChainBufferCount + gNumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + gNumFrameResources + gNumFrameResources;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +gNumFrameResources DSV for z-buffer.
	mZBufferDsvOffset = 1;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1 + gNumFrameResources;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

bool SSAO::Initialize()
{
	if (!MyApp::Initialize())return false;

	std::unique_ptr<RenderTexture>normalBuffer =
		std::make_unique<NormalBuffer>(md3dDevice.Get(), mClientWidth, mClientHeight);

	std::unique_ptr<RenderTexture>zBuffer =
		std::make_unique<ZBuffer>(md3dDevice.Get(), mClientWidth, mClientHeight);

	std::unique_ptr<SsaoMap>ssaoMap =
		std::make_unique<SsaoMap>(md3dDevice.Get(), mClientWidth, mClientHeight);

	std::unique_ptr<RandomVectorMap>randomVectorMap =
		std::make_unique<RandomVectorMap>(md3dDevice.Get(), 256, 256);

	mGbuffer[0] = std::move(normalBuffer);
	mGbuffer[1] = std::move(zBuffer);
	mSsaoMap = std::move(ssaoMap);
	mRandomVectorMap = std::move(randomVectorMap);

	mDebugViewerNormal = std::make_unique<DebugViewer>(md3dDevice, mCommandList, mBackBufferFormat, mCbvSrvUavDescriptorSize, gNumFrameResources);
	mDebugViewerNormal->SetTexSrv(mGbuffer[0]->Output(), mGbuffer[0]->SrvFormat());
	mDebugViewerNormal->SetPosition(DebugViewer::Position::Bottom0);

	mDebugViewerZ = std::make_unique<DebugViewer>(md3dDevice, mCommandList, mBackBufferFormat, mCbvSrvUavDescriptorSize, gNumFrameResources);
	mDebugViewerZ->SetTexSrv(mGbuffer[1]->Output(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
	mDebugViewerZ->SetPosition(DebugViewer::Position::Bottom1);

	mDebugViewerRandomVec = std::make_unique<DebugViewer>(md3dDevice, mCommandList, mBackBufferFormat, mCbvSrvUavDescriptorSize, gNumFrameResources);
	mDebugViewerRandomVec->SetTexSrv(mRandomVectorMap->Output(), mRandomVectorMap->SrvFormat());
	mDebugViewerRandomVec->SetPosition(DebugViewer::Position::Bottom2);

	mDebugViewerSsaoMap = std::make_unique<DebugViewer>(md3dDevice, mCommandList, mBackBufferFormat, mCbvSrvUavDescriptorSize, gNumFrameResources);
	mDebugViewerSsaoMap->SetTexSrv(mSsaoMap->Output(), mSsaoMap->SrvFormat());
	mDebugViewerSsaoMap->SetPosition(DebugViewer::Position::Bottom3);


	mCamera.SetPosition(XMFLOAT3(0, 5, -50));

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildRootSignature();
	BuildShadersAndInputLayout();
	LoadModels();
	BuildRenderItems();
	BuildFrameResources();

	BuildDescriptorHeaps();
	BuildBuffers();
	GenRandomVectorMap();
	BuildOffsetVectors();

	BuildPSOs();

	mCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	return true;
}

void SSAO::Update(const GameTimer& gt)
{
	MyApp::Update(gt);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

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

	// Update Gbuffer Pass(MainPass) Constant Buffer
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);

	// Update Ssao Pass CB
	{
		XMStoreFloat4x4(&mSsaoPassCB.Proj, XMMatrixTranspose(proj));

		// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
		XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);
		XMStoreFloat4x4(&mSsaoPassCB.ProjTex, XMMatrixTranspose(proj * T));

		XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
		XMStoreFloat4x4(&mSsaoPassCB.InvProj, XMMatrixTranspose(invProj));

		std::copy(&mOffsetVectors[0], &mOffsetVectors[14], &mSsaoPassCB.OffsetVectors[0]);

		mSsaoPassCB.OcclusionRadius = 0.5f;
		mSsaoPassCB.OcclusionFadeStart = 0.2f;
		mSsaoPassCB.OcclusionFadeEnd = 1.0f;
		mSsaoPassCB.SurfaceEpsilon = 0.05f;


		auto currPassCB = mCurrFrameResource->SsaoPassCB.get();
		currPassCB->CopyData(0, mSsaoPassCB);
	}
}

void SSAO::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	cmdListAlloc->Reset();

	// gen gbuffer
	{
		mCommandList->Reset(cmdListAlloc.Get(), mPSOs["gbuffer"].Get());

		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		auto normalBuffer = std::move(mGbuffer[0]);
		auto zBuffer = std::move(mGbuffer[1]);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalBuffer->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(zBuffer->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		mCommandList->ClearRenderTargetView(normalBuffer->Rtv(), Colors::LightSteelBlue, 0, nullptr);
		mCommandList->ClearDepthStencilView(zBuffer->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &normalBuffer->Rtv(), true, &zBuffer->Dsv());

		ID3D12DescriptorHeap* descriptorHeaps[] = { mHeapGbuffer.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		mCommandList->SetGraphicsRootSignature(mRootSignatureGbuffer.Get());

		int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
		auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapGbuffer->GetGPUDescriptorHandleForHeapStart());
		passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

		DrawRenderItems(mCommandList.Get(), mOpaqueRenderitems);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalBuffer->Output(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(zBuffer->Output(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));

		mGbuffer[0] = std::move(normalBuffer);
		mGbuffer[1] = std::move(zBuffer);
	}

	// gen ssao map
	{
		mCommandList->SetPipelineState(mPSOs["ssaoMap"].Get());

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));


		mCommandList->ClearRenderTargetView(mSsaoMap->Rtv(), Colors::Black, 0, nullptr);
		mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &mSsaoMap->Rtv(), true, &DepthStencilView());

		ID3D12DescriptorHeap* descriptorHeaps[] = { mHeapSsaoMap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		mCommandList->SetGraphicsRootSignature(mRootSignatureSsaoMap.Get());

		int passCbvIndex = mSsaoPassCbvOffset + mCurrFrameResourceIndex;
		auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapSsaoMap->GetGPUDescriptorHandleForHeapStart());
		passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(0, passCbvHandle);

		int gbufferSrvIndex = mNormalBufferSrvOffset + mCurrFrameResourceIndex;
		auto gbufferSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapSsaoMap->GetGPUDescriptorHandleForHeapStart());
		gbufferSrvHandle.Offset(gbufferSrvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(1, gbufferSrvHandle);

		int randomVecSrvIndex = mRandomVectorMapSrvOffset + mCurrFrameResourceIndex;
		auto randomVecSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapSsaoMap->GetGPUDescriptorHandleForHeapStart());
		randomVecSrvHandle.Offset(randomVecSrvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(2, randomVecSrvHandle);

		mCommandList->IASetVertexBuffers(0, 0, nullptr);
		mCommandList->IASetIndexBuffer(nullptr);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCommandList->DrawInstanced(6, 1, 0, 0);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
	}

	// debug view
	{
		{
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));

			mDebugViewerNormal->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		}

		{
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));

			mDebugViewerZ->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		}

		{
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap->Output(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));

			mDebugViewerRandomVec->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap->Output(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		}

		{
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));

			mDebugViewerSsaoMap->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		}
	}


	mCommandList->Close();

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

}

void SSAO::BuildFrameResources()
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

void SSAO::BuildRootSignature()
{
	// for gen gbuffer
	{
		CD3DX12_DESCRIPTOR_RANGE cbvTable[2];
		cbvTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		cbvTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable[0]);
		slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			sizeof(slotRootParameter) / sizeof(CD3DX12_ROOT_PARAMETER),
			slotRootParameter, 0, nullptr,
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
			IID_PPV_ARGS(mRootSignatureGbuffer.GetAddressOf())
		);
	}

	// for gen ssao map
	{
		CD3DX12_DESCRIPTOR_RANGE cbvTable;
		cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

		CD3DX12_DESCRIPTOR_RANGE gbufferTable;
		gbufferTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, gNumGBuffer, 0);

		CD3DX12_DESCRIPTOR_RANGE randomVecTable;
		randomVecTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
		slotRootParameter[1].InitAsDescriptorTable(1, &gbufferTable);
		slotRootParameter[2].InitAsDescriptorTable(1, &randomVecTable);

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
			IID_PPV_ARGS(mRootSignatureSsaoMap.GetAddressOf())
		);
	}
}

void SSAO::BuildShadersAndInputLayout()
{
	mShaders["gbufferVS"] = d3dUtil::CompileShader(L"Shaders\\SSAO\\gbuffer.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["gbufferPS"] = d3dUtil::CompileShader(L"Shaders\\SSAO\\gbuffer.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoMapVS"] = d3dUtil::CompileShader(L"Shaders\\SSAO\\ssaoMap.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoMapPS"] = d3dUtil::CompileShader(L"Shaders\\SSAO\\ssaoMap.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0} ,
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,sizeof(XMFLOAT3),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TANGENTU",0,DXGI_FORMAT_R32G32B32_FLOAT,0,sizeof(XMFLOAT3) * 2,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXC",0,DXGI_FORMAT_R32G32_FLOAT,0,sizeof(XMFLOAT3) * 3,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};
}

void SSAO::LoadModels()
{
	std::unique_ptr<Model> pacman = std::make_unique<Model>("pacman", "resource/pacman/pacman.stl", md3dDevice.Get(), mCommandList.Get());
	std::unique_ptr<Model> box = std::make_unique<Model>("box", "resource/box/box.stl", md3dDevice.Get(), mCommandList.Get());

	mModels[pacman->Geo()->Name] = std::move(pacman);
	mModels[box->Geo()->Name] = std::move(box);
}

void SSAO::BuildRenderItems()
{
	int objCBIndex = 0;
	for (auto& drawArg : mModels["pacman"]->Geo()->DrawArgs) {
		auto renderItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&renderItem->World, XMMatrixRotationRollPitchYaw(XMConvertToRadians(90), 0.0f, 0.0f));
		renderItem->ObjCBIndex = objCBIndex++;
		renderItem->Geo = const_cast<MeshGeometry*>(mModels["pacman"]->Geo());
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->IndexCount = drawArg.second.IndexCount;
		renderItem->StartIndexLocation = drawArg.second.StartIndexLocation;
		renderItem->BaseVertexLocation = drawArg.second.BaseVertexLocation;
		mAllRenderitems.push_back(std::move(renderItem));
	}

	for (auto& drawArg : mModels["box"]->Geo()->DrawArgs) {
		auto renderItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&renderItem->World, XMMatrixScaling(100, 1, 100) * XMMatrixTranslation(0, -20, 0));
		renderItem->ObjCBIndex = objCBIndex++;
		renderItem->Geo = const_cast<MeshGeometry*>(mModels["box"]->Geo());
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->IndexCount = drawArg.second.IndexCount;
		renderItem->StartIndexLocation = drawArg.second.StartIndexLocation;
		renderItem->BaseVertexLocation = drawArg.second.BaseVertexLocation;
		mAllRenderitems.push_back(std::move(renderItem));
	}

	// All the render items are opaque.
	for (auto& e : mAllRenderitems) mOpaqueRenderitems.push_back(e.get());
}

void SSAO::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)mOpaqueRenderitems.size();
	UINT frameCount = gNumFrameResources;

	UINT numObjFrameDescriptors = objCount * gNumFrameResources + frameCount;

	mPassCbvOffset = objCount * gNumFrameResources;

	// for gen gbuffer
	{
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
		cbvHeapDesc.NumDescriptors = numObjFrameDescriptors;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.NodeMask = 0;
		md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mHeapGbuffer));
	}

	// For gen ssao map
	{
		mSsaoPassCbvOffset = 0;
		UINT numPassCB = gNumFrameResources;

		mNormalBufferSrvOffset = mSsaoPassCbvOffset + numPassCB;
		UINT numNormalBuffer = 1 * gNumFrameResources;

		mZBufferSrvOffset = mNormalBufferSrvOffset + numNormalBuffer;
		UINT numZBuffer = 1 * gNumFrameResources;

		mRandomVectorMapSrvOffset = mZBufferSrvOffset + numZBuffer;
		UINT numRandomVectorMap = 1 * gNumFrameResources;

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
		heapDesc.NumDescriptors = numPassCB + numNormalBuffer + numZBuffer + numRandomVectorMap;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;
		md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeapSsaoMap));
	}
}

void SSAO::BuildBuffers()
{
	// for gen gbuffer
	{
		UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
		UINT objCount = mOpaqueRenderitems.size();

		for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {
			auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();

			for (int objectIndex = 0; objectIndex < objCount; objectIndex++) {
				D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();
				cbAddress += objectIndex * objCBByteSize;

				int heapIndex = objCount * frameIndex + objectIndex;
				auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeapGbuffer->GetCPUDescriptorHandleForHeapStart());
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
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeapGbuffer->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = passCBByteSize;
			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	// for gen ssao map
	{
		UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SsaoPassConstants));

		for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {
			auto passCB = mFrameResources[frameIndex]->SsaoPassCB->Resource();

			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

			int heapIndex = mSsaoPassCbvOffset + frameIndex;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeapSsaoMap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = passCBByteSize;
			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	// gen render textures
	{
		auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
		auto rtvCpuStart = mRtvHeap->GetCPUDescriptorHandleForHeapStart();

		auto ssaoSrvCpuStart = mHeapSsaoMap->GetCPUDescriptorHandleForHeapStart();
		auto ssaoSrvGpuStart = mHeapSsaoMap->GetGPUDescriptorHandleForHeapStart();

		auto normalBuffer = std::move(mGbuffer[0]);
		auto zBuffer = std::move(mGbuffer[1]);

		// for gen ssao map
		for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {
			normalBuffer->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(ssaoSrvCpuStart, mNormalBufferSrvOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(ssaoSrvGpuStart, mNormalBufferSrvOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, mNormalBufferRtvOffset + frameIndex, mRtvDescriptorSize));

			zBuffer->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(ssaoSrvCpuStart, mZBufferSrvOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(ssaoSrvGpuStart, mZBufferSrvOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, mZBufferDsvOffset + frameIndex, mDsvDescriptorSize), 
				CD3DX12_CPU_DESCRIPTOR_HANDLE());

			mRandomVectorMap->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(ssaoSrvCpuStart, mRandomVectorMapSrvOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(ssaoSrvGpuStart, mRandomVectorMapSrvOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE());
		}


		for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {
			mSsaoMap->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, mSsaoMapRtvOffset + frameIndex, mRtvDescriptorSize));
		}

		mGbuffer[0] = std::move(normalBuffer);
		mGbuffer[1] = std::move(zBuffer);
	}
}

void SSAO::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferPsoDesc;

	ZeroMemory(&gbufferPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	gbufferPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	gbufferPsoDesc.pRootSignature = mRootSignatureGbuffer.Get();
	gbufferPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["gbufferVS"]->GetBufferPointer()),
		mShaders["gbufferVS"]->GetBufferSize()
	};
	gbufferPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["gbufferPS"]->GetBufferPointer()),
		mShaders["gbufferPS"]->GetBufferSize()
	};

	gbufferPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gbufferPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gbufferPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gbufferPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gbufferPsoDesc.SampleMask = UINT_MAX;
	gbufferPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gbufferPsoDesc.NumRenderTargets = 1;
	gbufferPsoDesc.RTVFormats[0] = mGbuffer[0]->Format();
	gbufferPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	gbufferPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	gbufferPsoDesc.DSVFormat = mGbuffer[1]->DsvFormat();
	md3dDevice->CreateGraphicsPipelineState(&gbufferPsoDesc, IID_PPV_ARGS(&mPSOs["gbuffer"]));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = gbufferPsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["gbuffer_wireframe"]));

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoMapPsoDesc = gbufferPsoDesc;
		ssaoMapPsoDesc.pRootSignature = mRootSignatureSsaoMap.Get();
		ssaoMapPsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["ssaoMapVS"]->GetBufferPointer()),
			mShaders["ssaoMapVS"]->GetBufferSize()
		};
		ssaoMapPsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["ssaoMapPS"]->GetBufferPointer()),
			mShaders["ssaoMapPS"]->GetBufferSize()
		};
		ssaoMapPsoDesc.NumRenderTargets = 1;
		ssaoMapPsoDesc.RTVFormats[0] = mSsaoMap->Format();

		md3dDevice->CreateGraphicsPipelineState(&ssaoMapPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoMap"]));
	}
	
}

void SSAO::BuildOffsetVectors()
{
	// 8 cube corners
	mOffsetVectors[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	mOffsetVectors[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

	mOffsetVectors[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	mOffsetVectors[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

	mOffsetVectors[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	mOffsetVectors[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

	mOffsetVectors[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	mOffsetVectors[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

	// 6 centers of cube faces
	mOffsetVectors[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	mOffsetVectors[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

	mOffsetVectors[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	mOffsetVectors[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

	mOffsetVectors[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	mOffsetVectors[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int i = 0; i < 14; ++i)
	{
		// Create random lengths in [0.25, 1.0).
		float s = MathHelper::RandF(0.25f, 1.0f);

		XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsetVectors[i]));

		XMStoreFloat4(&mOffsetVectors[i], v);
	}
}

void SSAO::GenRandomVectorMap()
{
	XMCOLOR initData[256 * 256];
	for (int i = 0; i < 256; ++i)
	{
		for (int j = 0; j < 256; ++j)
		{
			// Random vector in [0,1).
			XMFLOAT3 v(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());

			initData[i * 256 + j] = XMCOLOR(v.x, v.y, v.z, 0.0f);
		}
	}

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = 256 * sizeof(XMCOLOR);
	subResourceData.SlicePitch = subResourceData.RowPitch * 256;

	//
	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	//

	auto randomVectorMapDesc = mRandomVectorMap->Output()->GetDesc();
	const UINT num2DSubresources = randomVectorMapDesc.DepthOrArraySize * randomVectorMapDesc.MipLevels;
	//const UINT num2DSubresources = 1 * 1;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap->Output(), 0, num2DSubresources);

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mRandomVectorMapUploadBuffer.GetAddressOf())));


	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap->Output(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(mCommandList.Get(), mRandomVectorMap->Output(), mRandomVectorMapUploadBuffer.Get(),
		0, 0, num2DSubresources, &subResourceData);
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap->Output(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
}

void SSAO::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
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
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapGbuffer->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}


