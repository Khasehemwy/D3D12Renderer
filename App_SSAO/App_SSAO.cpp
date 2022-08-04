#include "Common/MathHelper.h"
#include "Common/UploadBuffer.h"
#include "Common/GeometryGenerator.h"
#include "MyApp.h"
#include "Model.h"
#include "RenderTexture.h"
#include "DebugViewer.h"
#include "Toolkit.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 1;
const int gNumGBuffer = 3;
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
	DirectX::XMFLOAT4X4 NormalMatrixWorld;
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

struct BlurPassConstants
{
	XMFLOAT4X4 Proj;
	float BlurRadius;
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
		BlurPassCB = std::make_unique<UploadBuffer<BlurPassConstants>>(device, passCount, true);
		ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	};

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<SsaoPassConstants>> SsaoPassCB = nullptr;
	std::unique_ptr<UploadBuffer<BlurPassConstants>> BlurPassCB = nullptr;
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
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		md3dDevice->CreateShaderResourceView(mRenderTex.Get(), &srvDesc, mhCpuSrv);

		md3dDevice->CreateRenderTargetView(mRenderTex.Get(), nullptr, mhCpuRtv);
	}
};

class UAVTex :public RenderTexture
{
public:
	UAVTex(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
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

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		md3dDevice->CreateUnorderedAccessView(mRenderTex.Get(), nullptr, &uavDesc, mhCpuUav);
	}
};

class ScreenColor : public RenderTexture
{
public:
	ScreenColor(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
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
		md3dDevice->CreateShaderResourceView(mRenderTex.Get(), &srvDesc, mhCpuSrv);


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
	std::unique_ptr<DebugViewer> mDebugViewerRandomVec;
	std::unique_ptr<DebugViewer> mDebugViewerSsaoMap;
	std::unique_ptr<DebugViewer> mDebugViewerSsaoMapBlur;
	std::unique_ptr<DebugViewer> mDebugViewerScreenColor;

	virtual void CreateRtvAndDsvDescriptorHeaps()override;

	std::array<std::unique_ptr<RenderTexture>, gNumGBuffer>mGbuffer;
	// gbuffer[0]: normal
	// gbuffer[1]: z
	// gbuffer[2]: color
	std::unique_ptr<SsaoMap> mSsaoMap;
	std::unique_ptr<UAVTex> mSsaoMapBlur;

	ComPtr<ID3D12RootSignature> mRootSignatureGbuffer = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignatureSsaoMap = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignatureBlur = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignaturePresent = nullptr;
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
	UINT mScreenColorRtvOffset = 0;
	UINT mZBufferSrvOffset = 0;
	UINT mZBufferDsvOffset = 0;
	UINT mSsaoMapSrvOffset = 0;
	UINT mSsaoMapRtvOffset = 0;
	UINT mRandomVectorMapSrvOffset = 0;
	UINT mBlurPassCbvOffset = 0;
	UINT mNormalBufferBlurOffset = 0;
	UINT mZBufferBlurOffset = 0;
	UINT mSsaoMapBlurOffset = 0;
	UINT mOutputTexBlurOffset = 0;
	UINT mPresentColorTexOffset = 0;
	UINT mPresentSsaoMapOffset = 0;
	ComPtr<ID3D12DescriptorHeap> mHeapGbuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> mHeapSsaoMap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mHeapBlur = nullptr;
	ComPtr<ID3D12DescriptorHeap> mHeapPresent = nullptr;
	PassConstants mMainPassCB;
	SsaoPassConstants mSsaoPassCB;
	BlurPassConstants mBlurPassCB;
	std::unique_ptr<UploadBuffer<float>> mBlurWeightsBuffer = nullptr;
	void BuildDescriptorHeaps();
	void BuildBuffers();

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	void BuildPSOs();

	XMFLOAT4 mOffsetVectors[14];
	void BuildOffsetVectors();

	std::unique_ptr<RandomVectorMap> mRandomVectorMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer;
	void GenRandomVectorMap();

	std::vector<float> mBlurWeights;

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
	// Add +gNumFrameResources RTV for screenColor.
	mNormalBufferRtvOffset = SwapChainBufferCount;
	mSsaoMapRtvOffset = SwapChainBufferCount + gNumFrameResources;
	mScreenColorRtvOffset = SwapChainBufferCount + gNumFrameResources + gNumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + gNumFrameResources + gNumFrameResources + gNumFrameResources;
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

	std::wostringstream title;
	title << L"SSAO";
	mMainWndCaption = title.str();

	std::unique_ptr<RenderTexture>normalBuffer =
		std::make_unique<NormalBuffer>(md3dDevice.Get(), mClientWidth, mClientHeight);

	std::unique_ptr<RenderTexture>zBuffer =
		std::make_unique<ZBuffer>(md3dDevice.Get(), mClientWidth, mClientHeight);

	std::unique_ptr<SsaoMap>ssaoMap =
		std::make_unique<SsaoMap>(md3dDevice.Get(), mClientWidth, mClientHeight);

	std::unique_ptr<RandomVectorMap>randomVectorMap =
		std::make_unique<RandomVectorMap>(md3dDevice.Get(), 256, 256);

	std::unique_ptr<UAVTex>ssaoMapBlur =
		std::make_unique<UAVTex>(md3dDevice.Get(), mClientWidth, mClientHeight);

	std::unique_ptr<ScreenColor>screenColor =
		std::make_unique<ScreenColor>(md3dDevice.Get(), mClientWidth, mClientHeight);

	mGbuffer[0] = std::move(normalBuffer);
	mGbuffer[1] = std::move(zBuffer);
	mGbuffer[2] = std::move(screenColor);
	mSsaoMap = std::move(ssaoMap);
	mRandomVectorMap = std::move(randomVectorMap);
	mSsaoMapBlur = std::move(ssaoMapBlur);

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
	mDebugViewerSsaoMap->SetPosition(DebugViewer::Position::Bottom1);

	mDebugViewerSsaoMapBlur = std::make_unique<DebugViewer>(md3dDevice, mCommandList, mBackBufferFormat, mCbvSrvUavDescriptorSize, gNumFrameResources);
	mDebugViewerSsaoMapBlur->SetTexSrv(mSsaoMapBlur->Output(), mSsaoMapBlur->SrvFormat());
	mDebugViewerSsaoMapBlur->SetPosition(DebugViewer::Position::Bottom2);

	mDebugViewerScreenColor = std::make_unique<DebugViewer>(md3dDevice, mCommandList, mBackBufferFormat, mCbvSrvUavDescriptorSize, gNumFrameResources);
	mDebugViewerScreenColor->SetTexSrv(mGbuffer[2]->Output(), mGbuffer[2]->SrvFormat());
	mDebugViewerScreenColor->SetPosition(DebugViewer::Position::Bottom3);

	mBlurWeights = Toolkit::CalcGaussWeights(2.5f);

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

	XMMATRIX view = XMLoadFloat4x4(&mView);

	//Update Per Object CB
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRenderitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			XMMATRIX normalMatrixWorld = XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(world), world));

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.NormalMatrixWorld, XMMatrixTranspose(normalMatrixWorld));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}

	// Update Gbuffer Pass(MainPass) Constant Buffer
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

	// Update Blur Pass CB
	{
		mBlurPassCB.BlurRadius = floor((mBlurWeights.size()) / 2);
		XMStoreFloat4x4(&mBlurPassCB.Proj, XMMatrixTranspose(proj));

		auto currPassCB = mCurrFrameResource->BlurPassCB.get();
		currPassCB->CopyData(0, mBlurPassCB);

		for (int i = 0; i < mBlurWeights.size(); i++) {
			mBlurWeightsBuffer->CopyData(i, mBlurWeights[i]);
		}
	}
}

void SSAO::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	cmdListAlloc->Reset();

	// gen gbuffer
	{
		mCommandList->Reset(cmdListAlloc.Get(), mPSOs["gbuffer"].Get());

		mCommandList->RSSetViewports(1, &mGbuffer[0]->Viewport());
		mCommandList->RSSetScissorRects(1, &mGbuffer[0]->ScissorRect());

		auto normalBuffer = std::move(mGbuffer[0]);
		auto zBuffer = std::move(mGbuffer[1]);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalBuffer->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(zBuffer->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		mCommandList->ClearRenderTargetView(normalBuffer->Rtv(), Colors::Black, 0, nullptr);
		mCommandList->ClearRenderTargetView(mGbuffer[2]->Rtv(), Colors::LightSteelBlue, 0, nullptr);
		mCommandList->ClearDepthStencilView(zBuffer->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> renderTargets =
		{
			{ normalBuffer->Rtv() },
			{ mGbuffer[2]->Rtv()}
		};
		mCommandList->OMSetRenderTargets(2, renderTargets.data(), false, &zBuffer->Dsv());

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
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(zBuffer->Output(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COMMON));

		mGbuffer[0] = std::move(normalBuffer);
		mGbuffer[1] = std::move(zBuffer);
	}

	// gen ssao map
	{
		mCommandList->RSSetViewports(1, &mSsaoMap->Viewport());
		mCommandList->RSSetScissorRects(1, &mSsaoMap->ScissorRect());

		mCommandList->SetPipelineState(mPSOs["ssaoMap"].Get());

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
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
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
	}

	// blur ssao map
	{
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMapBlur->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		mCommandList->SetPipelineState(mPSOs["blur"].Get());

		ID3D12DescriptorHeap* descriptorHeaps[] = { mHeapBlur.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		mCommandList->SetComputeRootSignature(mRootSignatureBlur.Get());

		int passCbvIndex = mBlurPassCbvOffset + mCurrFrameResourceIndex;
		auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapBlur->GetGPUDescriptorHandleForHeapStart());
		passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetComputeRootDescriptorTable(0, passCbvHandle);

		int gbufferSrvIndex = mNormalBufferBlurOffset + mCurrFrameResourceIndex;
		auto gbufferSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapBlur->GetGPUDescriptorHandleForHeapStart());
		gbufferSrvHandle.Offset(gbufferSrvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetComputeRootDescriptorTable(1, gbufferSrvHandle);

		int ssaoMapSrvIndex = mSsaoMapBlurOffset + mCurrFrameResourceIndex;
		auto ssaoMapSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapBlur->GetGPUDescriptorHandleForHeapStart());
		ssaoMapSrvHandle.Offset(ssaoMapSrvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetComputeRootDescriptorTable(2, ssaoMapSrvHandle);

		int blurOutUavIndex = mOutputTexBlurOffset + mCurrFrameResourceIndex;
		auto blurOutUavHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapBlur->GetGPUDescriptorHandleForHeapStart());
		blurOutUavHandle.Offset(blurOutUavIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetComputeRootDescriptorTable(3, blurOutUavHandle);

		auto blurWeightsBuffer = mBlurWeightsBuffer->Resource();
		mCommandList->SetComputeRootShaderResourceView(4, blurWeightsBuffer->GetGPUVirtualAddress());

		UINT numGroupX = (UINT)ceilf(mClientWidth / 256.0f);
		mCommandList->Dispatch(numGroupX, mClientHeight, 1);


		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMapBlur->Output(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
	}

	// final present
	{
		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		mCommandList->SetPipelineState(mPSOs["present"].Get());

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMapBlur->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));


		mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
		mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

		ID3D12DescriptorHeap* descriptorHeaps[] = { mHeapPresent.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		mCommandList->SetGraphicsRootSignature(mRootSignaturePresent.Get());

		int colorSrvIndex = mPresentColorTexOffset + mCurrFrameResourceIndex;
		auto colorSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapPresent->GetGPUDescriptorHandleForHeapStart());
		colorSrvHandle.Offset(colorSrvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(0, colorSrvHandle);

		int ssaoMapSrvIndex = mPresentSsaoMapOffset + mCurrFrameResourceIndex;
		auto ssaoMapSrvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapPresent->GetGPUDescriptorHandleForHeapStart());
		ssaoMapSrvHandle.Offset(ssaoMapSrvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(1, ssaoMapSrvHandle);

		mCommandList->IASetVertexBuffers(0, 0, nullptr);
		mCommandList->IASetIndexBuffer(nullptr);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mCommandList->DrawInstanced(6, 1, 0, 0);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMapBlur->Output(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	}

	// debug view
	{
		{
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			mDebugViewerNormal->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[0]->Output(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		}

		//{
		//	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
		//		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		//	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		//		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		//	mDebugViewerZ->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

		//	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[1]->Output(),
		//		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
		//  mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		//		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		//}

		{
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			mDebugViewerSsaoMap->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMap->Output(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		}

		{
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMapBlur->Output(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			mDebugViewerSsaoMapBlur->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSsaoMapBlur->Output(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		}

		{
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			mDebugViewerScreenColor->Draw(CurrentBackBufferView(), mCurrFrameResourceIndex);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mGbuffer[2]->Output(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON));
			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
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
		randomVecTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);

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

	// for final present
	{
		CD3DX12_DESCRIPTOR_RANGE colorTexTable;
		colorTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE ssaoMapTable;
		ssaoMapTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &colorTexTable);
		slotRootParameter[1].InitAsDescriptorTable(1, &ssaoMapTable);

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

	// for blur
	{
		CD3DX12_DESCRIPTOR_RANGE perPassCbTable;
		perPassCbTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE gbufferTable;
		gbufferTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, gNumGBuffer, 0);

		CD3DX12_DESCRIPTOR_RANGE ssaoMapTable;
		ssaoMapTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);

		CD3DX12_DESCRIPTOR_RANGE outputTexTable;
		outputTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[5];
		slotRootParameter[0].InitAsDescriptorTable(1, &perPassCbTable);
		slotRootParameter[1].InitAsDescriptorTable(1, &gbufferTable);
		slotRootParameter[2].InitAsDescriptorTable(1, &ssaoMapTable);
		slotRootParameter[3].InitAsDescriptorTable(1, &outputTexTable);
		slotRootParameter[4].InitAsShaderResourceView(4);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			sizeof(slotRootParameter) / sizeof(CD3DX12_ROOT_PARAMETER),
			slotRootParameter,
			0, nullptr,
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
			IID_PPV_ARGS(mRootSignatureBlur.GetAddressOf())
		);
	}
}

void SSAO::BuildShadersAndInputLayout()
{
	mShaders["gbufferVS"] = d3dUtil::CompileShader(L"..\\Shaders\\SSAO\\gbuffer.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["gbufferPS"] = d3dUtil::CompileShader(L"..\\Shaders\\SSAO\\gbuffer.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoMapVS"] = d3dUtil::CompileShader(L"..\\Shaders\\SSAO\\ssaoMap.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoMapPS"] = d3dUtil::CompileShader(L"..\\Shaders\\SSAO\\ssaoMap.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["presentVS"] = d3dUtil::CompileShader(L"..\\Shaders\\SSAO\\present.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["presentPS"] = d3dUtil::CompileShader(L"..\\Shaders\\SSAO\\present.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["blurCS"] = d3dUtil::CompileShader(L"..\\Shaders\\SSAO\\blur_cs.hlsl", nullptr, "CS", "cs_5_1");

	mInputLayout = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0} ,
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,sizeof(XMFLOAT3),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TANGENTU",0,DXGI_FORMAT_R32G32B32_FLOAT,0,sizeof(XMFLOAT3) * 2,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXC",0,DXGI_FORMAT_R32G32_FLOAT,0,sizeof(XMFLOAT3) * 3,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};
}

void SSAO::LoadModels()
{
	std::unique_ptr<Model> pacman = std::make_unique<Model>("pacman", "../resources/pacman/pacman.stl", md3dDevice.Get(), mCommandList.Get());
	std::unique_ptr<Model> box = std::make_unique<Model>("box", "../resources/box/box.stl", md3dDevice.Get(), mCommandList.Get());

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
		XMMATRIX world = XMMatrixScaling(100, 100, 100);
		world *= XMMatrixTranslation(0, -115, 0);
		XMStoreFloat4x4(&renderItem->World, world);
		renderItem->ObjCBIndex = objCBIndex++;
		renderItem->Geo = const_cast<MeshGeometry*>(mModels["box"]->Geo());
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->IndexCount = drawArg.second.IndexCount;
		renderItem->StartIndexLocation = drawArg.second.StartIndexLocation;
		renderItem->BaseVertexLocation = drawArg.second.BaseVertexLocation;
		mAllRenderitems.push_back(std::move(renderItem));
	}

	for (auto& drawArg : mModels["box"]->Geo()->DrawArgs) {
		auto renderItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&renderItem->World, XMMatrixTranslation(-5, -1, 2) * XMMatrixScaling(10, 10, 10));
		renderItem->ObjCBIndex = objCBIndex++;
		renderItem->Geo = const_cast<MeshGeometry*>(mModels["box"]->Geo());
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->IndexCount = drawArg.second.IndexCount;
		renderItem->StartIndexLocation = drawArg.second.StartIndexLocation;
		renderItem->BaseVertexLocation = drawArg.second.BaseVertexLocation;
		mAllRenderitems.push_back(std::move(renderItem));
	}

	for (auto& drawArg : mModels["box"]->Geo()->DrawArgs) {
		auto renderItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&renderItem->World, XMMatrixTranslation(-3, -1, 4) * XMMatrixScaling(10, 10, 10));
		renderItem->ObjCBIndex = objCBIndex++;
		renderItem->Geo = const_cast<MeshGeometry*>(mModels["box"]->Geo());
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->IndexCount = drawArg.second.IndexCount;
		renderItem->StartIndexLocation = drawArg.second.StartIndexLocation;
		renderItem->BaseVertexLocation = drawArg.second.BaseVertexLocation;
		mAllRenderitems.push_back(std::move(renderItem));
	}

	for (auto& drawArg : mModels["box"]->Geo()->DrawArgs) {
		auto renderItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&renderItem->World, XMMatrixTranslation(-5, -1, 4) * XMMatrixScaling(10, 10, 10));
		renderItem->ObjCBIndex = objCBIndex++;
		renderItem->Geo = const_cast<MeshGeometry*>(mModels["box"]->Geo());
		renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		renderItem->IndexCount = drawArg.second.IndexCount;
		renderItem->StartIndexLocation = drawArg.second.StartIndexLocation;
		renderItem->BaseVertexLocation = drawArg.second.BaseVertexLocation;
		mAllRenderitems.push_back(std::move(renderItem));
	}

	for (auto& drawArg : mModels["box"]->Geo()->DrawArgs) {
		auto renderItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&renderItem->World, XMMatrixTranslation(-5, 1, 4) * XMMatrixScaling(10, 10, 10));
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

	// For final present
	{
		mPresentColorTexOffset = 0;
		UINT numColorTex = gNumFrameResources;

		mPresentSsaoMapOffset = mPresentColorTexOffset + numColorTex;
		UINT numSsaoMap = gNumFrameResources;

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
		heapDesc.NumDescriptors = numColorTex + numSsaoMap;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;
		md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeapPresent));
	}

	// for ssao map blur 
	{
		mBlurPassCbvOffset = 0;
		UINT numBlurPassCbv = gNumFrameResources;

		mNormalBufferBlurOffset = mBlurPassCbvOffset + numBlurPassCbv;
		UINT numNormalBuffer = 1 * gNumFrameResources;

		mZBufferBlurOffset = mNormalBufferBlurOffset + numNormalBuffer;
		UINT numZBuffer = 1 * gNumFrameResources;

		mSsaoMapBlurOffset = mZBufferBlurOffset + numZBuffer;
		UINT numSsaoMap = 1 * gNumFrameResources;

		mOutputTexBlurOffset = mSsaoMapBlurOffset + numSsaoMap;
		UINT numOutputtex = 1 * gNumFrameResources;

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
		heapDesc.NumDescriptors = numBlurPassCbv + numNormalBuffer + numZBuffer + numSsaoMap + numOutputtex;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.NodeMask = 0;
		md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeapBlur));
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

	// for blur
	{
		UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(BlurPassConstants));

		for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {
			auto passCB = mFrameResources[frameIndex]->BlurPassCB->Resource();

			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

			int heapIndex = mBlurPassCbvOffset + frameIndex;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mHeapBlur->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = passCBByteSize;
			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}

		mBlurWeightsBuffer = std::make_unique<UploadBuffer<float>>(md3dDevice.Get(), mBlurWeights.size(), false);
	}

	// gen render textures
	{
		auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
		auto rtvCpuStart = mRtvHeap->GetCPUDescriptorHandleForHeapStart();

		auto ssaoSrvCpuStart = mHeapSsaoMap->GetCPUDescriptorHandleForHeapStart();
		auto ssaoSrvGpuStart = mHeapSsaoMap->GetGPUDescriptorHandleForHeapStart();

		auto blurSrvCpuStart = mHeapBlur->GetCPUDescriptorHandleForHeapStart();
		auto blurSrvGpuStart = mHeapBlur->GetGPUDescriptorHandleForHeapStart();
		auto blurUavCpuStart = mHeapBlur->GetCPUDescriptorHandleForHeapStart();

		auto presentSrvCpuStart = mHeapPresent->GetCPUDescriptorHandleForHeapStart();
		auto presentSrvGpuStart = mHeapPresent->GetGPUDescriptorHandleForHeapStart();

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

			mGbuffer[2]->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(presentSrvCpuStart, mPresentColorTexOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(presentSrvGpuStart, mPresentColorTexOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, mScreenColorRtvOffset + frameIndex, mRtvDescriptorSize));
		}


		// for blur
		for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {
			normalBuffer->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(blurSrvCpuStart, mNormalBufferBlurOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(blurSrvGpuStart, mNormalBufferBlurOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, mNormalBufferRtvOffset + frameIndex, mRtvDescriptorSize));

			zBuffer->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(blurSrvCpuStart, mZBufferBlurOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(blurSrvGpuStart, mZBufferBlurOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, mZBufferDsvOffset + frameIndex, mDsvDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE());

			mSsaoMap->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(blurSrvCpuStart, mSsaoMapBlurOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(blurSrvGpuStart, mSsaoMapBlurOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvCpuStart, mSsaoMapRtvOffset + frameIndex, mRtvDescriptorSize));

			mSsaoMapBlur->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(presentSrvCpuStart, mPresentSsaoMapOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(presentSrvGpuStart, mPresentSsaoMapOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(blurUavCpuStart, mOutputTexBlurOffset + frameIndex, mCbvSrvUavDescriptorSize));
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
	gbufferPsoDesc.NumRenderTargets = 2;
	gbufferPsoDesc.RTVFormats[0] = mGbuffer[0]->Format();
	gbufferPsoDesc.RTVFormats[1] = mGbuffer[2]->Format();
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
		ssaoMapPsoDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;

		md3dDevice->CreateGraphicsPipelineState(&ssaoMapPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoMap"]));
	}

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC presentPsoDesc = gbufferPsoDesc;
		presentPsoDesc.InputLayout = { nullptr, 0 };
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
		presentPsoDesc.DepthStencilState.DepthEnable = false;
		presentPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		presentPsoDesc.NumRenderTargets = 1;
		presentPsoDesc.RTVFormats[0] = mBackBufferFormat;
		presentPsoDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;

		md3dDevice->CreateGraphicsPipelineState(&presentPsoDesc, IID_PPV_ARGS(&mPSOs["present"]));
	}

	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC blurPsoDesc;
		ZeroMemory(&blurPsoDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));
		blurPsoDesc.pRootSignature = mRootSignatureBlur.Get();
		blurPsoDesc.CS = {
			reinterpret_cast<BYTE*>(mShaders["blurCS"]->GetBufferPointer()),
			mShaders["blurCS"]->GetBufferSize()
		};
		blurPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		md3dDevice->CreateComputePipelineState(&blurPsoDesc, IID_PPV_ARGS(&mPSOs["blur"]));
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


