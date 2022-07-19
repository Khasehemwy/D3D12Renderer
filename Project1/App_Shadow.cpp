#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "MyApp.h"
#include "RenderTexture.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 1;

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT4 color;
	XMFLOAT3 normal;
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
	int lightCount;
	XMFLOAT3 eyePos;
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

struct ShadowMapUse 
{
	XMFLOAT4X4 view;
	XMFLOAT4X4 proj;
};

class ShadowMap : public RenderTexture
{
public:
	ShadowMap(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		:RenderTexture(device, width, height, format, flag)
	{};

	virtual void BuildDescriptors()override;
};

void ShadowMap::BuildDescriptors()
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

class Shadow :public MyApp
{
public:
	Shadow(HINSTANCE hInstance);

	virtual bool Initialize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnResize()override;
	virtual void OnKeyboardInput(const GameTimer& gt)override;
private:
	enum class DefaultPSO : int
	{
		perObjectCB = 0,
		perPassCB,
		lightsSRV,
		lightViewProjsSRV,
		shadowMapSRV,
		size //simply get enum class's size
	};

	static const int mMaxLightNum = 1;
	std::unique_ptr<UploadBuffer<Light>> mLightBuffer = nullptr;
	std::unique_ptr<UploadBuffer<XMFLOAT4X4>> mLightShadowTransformBuffer = nullptr;
	std::vector<Light>mLights;
	std::vector<XMFLOAT4X4>mLightShadowTransforms;

	float mShadowMapWidth = 30;
	float mShadowMapHeight = 30;
	std::unique_ptr<ShadowMap> mShadowMap;
	std::unique_ptr<UploadBuffer<ShadowMapUse>> mShadowMapUseBuffer = nullptr;
	void GenShadowMap(int lightIndex);

	void DrawShadowMapToScreen();

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mShadowMapRootSignature = nullptr;
	void BuildRootSignature();

	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC>mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC>mInputLayoutShadowMap;
	void BuildShadersAndInputLayout();

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	void BuildShapeGeometry();

	std::vector<std::unique_ptr<RenderItem>> mAllRenderitems;
	std::vector<RenderItem*> mOpaqueRenderitems;
	void BuildObjects();

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	int mCurrFrameResourceIndex = 0;
	FrameResource* mCurrFrameResource = nullptr;
	void BuildFrameResources();

	UINT mPassCbvOffset = 0;
	UINT mShadowMapTexOffset = 0;
	UINT mShadowMapDsvOffset = 0;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	PassConstants mMainPassCB;
	void BuildDescriptorHeaps();
	void BuildBuffers();
	virtual void CreateRtvAndDsvDescriptorHeaps()override;

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
		Shadow theApp(hInstance);
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

Shadow::Shadow(HINSTANCE hInstance) :
	MyApp(hInstance)
{
}

bool Shadow::Initialize()
{
	if (!MyApp::Initialize())return false;

	mCamera.SetPosition(XMFLOAT3(0, 5, -10));

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	mShadowMap = std::make_unique<ShadowMap>(
		md3dDevice.Get(),
		mClientWidth, mClientHeight,
		DXGI_FORMAT_R24G8_TYPELESS, // Format must match Flag
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildObjects();
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

void Shadow::Update(const GameTimer& gt)
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
	mMainPassCB.lightCount = mMaxLightNum;
	mMainPassCB.eyePos = mCamera.GetPosition3f();

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);

	//Update Lights
	for (int i = 0; i < mMaxLightNum; i++) {
		mLightBuffer->CopyData(i, mLights[i]);
	}
}

void Shadow::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	cmdListAlloc->Reset();

	{
		mLightShadowTransforms.clear();

		GenShadowMap(0);
		
		for (int i = 0; i < mMaxLightNum; i++) {
			mLightShadowTransformBuffer->CopyData(i, mLightShadowTransforms[i]);
		}
	}

	// restore mMainPassCB
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
	
	//![Tip]: Using SetPipelineState() instead of Reset() cause GenShadowMap() Have to reset it.
	if (mIsWireframe) {
		mCommandList->SetPipelineState(mPSOs["opaque_wireframe"].Get());
	}
	else {
		mCommandList->SetPipelineState(mPSOs["opaque"].Get());
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
	mCommandList->SetGraphicsRootDescriptorTable((UINT)DefaultPSO::perPassCB, passCbvHandle);

	{
		int shadowMapTexIndex = mShadowMapTexOffset + mCurrFrameResourceIndex;
		auto shadowMapTexHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		shadowMapTexHandle.Offset(shadowMapTexIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable((UINT)DefaultPSO::shadowMapSRV, shadowMapTexHandle);
	}

	mCommandList->SetGraphicsRootShaderResourceView((UINT)DefaultPSO::lightsSRV, mLightBuffer->Resource()->GetGPUVirtualAddress());
	mCommandList->SetGraphicsRootShaderResourceView((UINT)DefaultPSO::lightViewProjsSRV, mLightShadowTransformBuffer->Resource()->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mOpaqueRenderitems);

	DrawShadowMapToScreen();

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

void Shadow::OnResize()
{
	MyApp::OnResize();

	if (mShadowMap != nullptr) {
		mShadowMap->OnResize(mClientWidth, mClientHeight);
	}
}

void Shadow::OnKeyboardInput(const GameTimer& gt)
{
	MyApp::OnKeyboardInput(gt);

	const float dt = gt.DeltaTime();
	if (GetAsyncKeyState('I') & 0x8000) {
		mShadowMapHeight -= 20.0f * dt;
	}
	if (GetAsyncKeyState('K') & 0x8000) {
		mShadowMapHeight += 20.0f * dt;
	}
	if (GetAsyncKeyState('J') & 0x8000) {
		mShadowMapWidth -= 20.0f * dt;
	}
	if (GetAsyncKeyState('L') & 0x8000) {
		mShadowMapWidth += 20.0f * dt;
	}
}

void Shadow::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(
			std::make_unique<FrameResource>(
				md3dDevice.Get(),
				gNumFrameResources,
				mAllRenderitems.size()
			)
		);
	};
}

void Shadow::GenShadowMap(int lightIndex)
{
	// Update Eye Pos to Light
	auto light = mLights[lightIndex];

	Camera lightCam = mCamera;
	lightCam.SetPosition(light.Position);
	lightCam.LookAt(light.Position, light.Direction, XMFLOAT3(0, 1, 0));
	lightCam.UpdateViewMatrix();
	XMMATRIX view = lightCam.GetView();

	auto orthoProj = XMMatrixOrthographicLH(mShadowMapWidth, mShadowMapHeight, 1.0f, 100.0f);

	{
		// transform to texture space
		XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		XMFLOAT4X4 tmp;
		auto shadowTransform = view * orthoProj * T;
		XMStoreFloat4x4(&tmp, XMMatrixTranspose(shadowTransform));
		mLightShadowTransforms.push_back(tmp);
	}
	// Update Finish

	{
		ShadowMapUse data;
		XMStoreFloat4x4(&data.view,XMMatrixTranspose(view));
		XMStoreFloat4x4(&data.proj, XMMatrixTranspose(orthoProj));
		mShadowMapUseBuffer->CopyData(0, data);
	}

	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	mCommandList->Reset(cmdListAlloc.Get(), mPSOs["shadowMap"].Get());

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
	mCommandList->SetGraphicsRootDescriptorTable((UINT)DefaultPSO::perPassCB, passCbvHandle);

	mCommandList->SetGraphicsRootShaderResourceView(2, mShadowMapUseBuffer->Resource()->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mOpaqueRenderitems);

	// copy to shadow map
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Output(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	mCommandList->CopyResource(mShadowMap->Output(), CurrentBackBuffer());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT));
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Output(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));


	//mCommandList->Close();

	//ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	//mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	//ThrowIfFailed(mSwapChain->Present(0, 0));
	//mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	//mCurrFrameResource->Fence = ++mCurrentFence;

	//mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void Shadow::DrawShadowMapToScreen()
{
	mCommandList->SetPipelineState(mPSOs["shadowMapPresent"].Get());

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable((UINT)DefaultPSO::perPassCB, passCbvHandle);

	{
		int shadowMapTexIndex = mShadowMapTexOffset + mCurrFrameResourceIndex;
		auto shadowMapTexHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		shadowMapTexHandle.Offset(shadowMapTexIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable((UINT)DefaultPSO::shadowMapSRV, shadowMapTexHandle);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		auto shadowMapTexCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		shadowMapTexCpuHandle.Offset(shadowMapTexIndex, mCbvSrvUavDescriptorSize);
		md3dDevice->CreateShaderResourceView(mShadowMap->Output(), &srvDesc, shadowMapTexCpuHandle);
	}

	mCommandList->SetGraphicsRootShaderResourceView((UINT)DefaultPSO::lightsSRV, mLightBuffer->Resource()->GetGPUVirtualAddress());
	mCommandList->SetGraphicsRootShaderResourceView((UINT)DefaultPSO::lightViewProjsSRV, mLightShadowTransformBuffer->Resource()->GetGPUVirtualAddress());

	mCommandList->IASetVertexBuffers(0, 1, &mGeometries["shadowMapGeo"]->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mGeometries["shadowMapGeo"]->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->DrawIndexedInstanced(
		mGeometries["shadowMapGeo"]->DrawArgs["shadowMap"].IndexCount,
		1, 0, 0, 0
	);
}

void Shadow::BuildRootSignature()
{
	{
		CD3DX12_DESCRIPTOR_RANGE cbvTable[2];
		cbvTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		cbvTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 4, 1);

		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameter[(int)DefaultPSO::size];
		slotRootParameter[(int)DefaultPSO::perObjectCB].InitAsDescriptorTable(1, &cbvTable[0]);
		slotRootParameter[(int)DefaultPSO::perPassCB].InitAsDescriptorTable(1, &cbvTable[1]);
		slotRootParameter[(int)DefaultPSO::lightsSRV].InitAsShaderResourceView(0, 1);
		slotRootParameter[(int)DefaultPSO::lightViewProjsSRV].InitAsShaderResourceView(0, 2);
		slotRootParameter[(int)DefaultPSO::shadowMapSRV].InitAsDescriptorTable(1, &texTable);

		auto staticSamplers = GetStaticSamplers();

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			sizeof(slotRootParameter) / sizeof(CD3DX12_ROOT_PARAMETER),
			slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

	// shadow map generate use
	{
		CD3DX12_DESCRIPTOR_RANGE cbvTable[2];
		cbvTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		cbvTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 4, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable[0]);
		slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable[1]);
		slotRootParameter[2].InitAsShaderResourceView(0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			sizeof(slotRootParameter) / sizeof(CD3DX12_ROOT_PARAMETER), slotRootParameter, 0, nullptr,
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
			IID_PPV_ARGS(mShadowMapRootSignature.GetAddressOf())
		);
	}
}

void Shadow::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Shadow\\shader.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Shadow\\shader.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["shadowMapPresentVS"] = d3dUtil::CompileShader(L"Shaders\\Shadow\\presentShadowMap.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowMapPresentPS"] = d3dUtil::CompileShader(L"Shaders\\Shadow\\presentShadowMap.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["shadowMapVS"] = d3dUtil::CompileShader(L"Shaders\\Shadow\\shadowMap.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowMapPS"] = d3dUtil::CompileShader(L"Shaders\\Shadow\\shadowMap.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0} ,
		{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,sizeof(float) * 3,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,sizeof(float) * 3 + sizeof(float) * 4,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};

	mInputLayoutShadowMap = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};
}

void Shadow::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.0f, 3.0f, 1, 1);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].pos = box.Vertices[i].Position;
		vertices[k].color = XMFLOAT4(DirectX::Colors::DarkGreen);
		vertices[k].normal = box.Vertices[i].Normal;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].pos = grid.Vertices[i].Position;
		vertices[k].color = XMFLOAT4(DirectX::Colors::ForestGreen);
		vertices[k].normal = grid.Vertices[i].Normal;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].pos = sphere.Vertices[i].Position;
		vertices[k].color = XMFLOAT4(DirectX::Colors::Crimson);
		vertices[k].normal = sphere.Vertices[i].Normal;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].pos = cylinder.Vertices[i].Position;
		vertices[k].color = XMFLOAT4(DirectX::Colors::SteelBlue);
		vertices[k].normal = cylinder.Vertices[i].Normal;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

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

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);


	// build shadow map geo
	{
		std::array<Vertex, 4> vertices =
		{
			Vertex({ XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(0,0,0,1),XMFLOAT3(0,0,0)}),//left top
			Vertex({ XMFLOAT3(1.0f, -0.5f, 0.0f), XMFLOAT4(0,0,0,1),XMFLOAT3(1,0,0)}),//right top
			Vertex({ XMFLOAT3(0.5f, -1.0f, 0.0f), XMFLOAT4(0,0,0,1),XMFLOAT3(0,1,0)}),//left bottom
			Vertex({ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT4(0,0,0,1),XMFLOAT3(1,1,0)}),//right bottom
		};
		std::array<std::uint16_t, 6> indices =
		{
			0,1,2,
			1,3,2
		};

		const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto shadowMapGeo = std::make_unique<MeshGeometry>();
		shadowMapGeo->Name = "shadowMapGeo";
		D3DCreateBlob(vbByteSize, &shadowMapGeo->VertexBufferCPU);
		CopyMemory(shadowMapGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
		D3DCreateBlob(ibByteSize, &shadowMapGeo->IndexBufferCPU);
		CopyMemory(shadowMapGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		shadowMapGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), vertices.data(), vbByteSize, shadowMapGeo->VertexBufferUploader);
		shadowMapGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
			mCommandList.Get(), indices.data(), ibByteSize, shadowMapGeo->IndexBufferUploader);

		shadowMapGeo->VertexByteStride = sizeof(Vertex);
		shadowMapGeo->VertexBufferByteSize = vbByteSize;
		shadowMapGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
		shadowMapGeo->IndexBufferByteSize = ibByteSize;

		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)indices.size();
		submesh.StartIndexLocation = 0;
		submesh.BaseVertexLocation = 0;

		shadowMapGeo->DrawArgs["shadowMap"] = submesh;
		mGeometries[shadowMapGeo->Name] = std::move(shadowMapGeo);
	}

}

void Shadow::BuildObjects()
{
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRenderitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	gridRitem->ObjCBIndex = 1;
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRenderitems.push_back(std::move(gridRitem));

	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mAllRenderitems.push_back(std::move(leftCylRitem));
		mAllRenderitems.push_back(std::move(rightCylRitem));
		mAllRenderitems.push_back(std::move(leftSphereRitem));
		mAllRenderitems.push_back(std::move(rightSphereRitem));
	}

	// All the render items are opaque.
	for (auto& e : mAllRenderitems) mOpaqueRenderitems.push_back(e.get());

	// Light
	{
		Light dirLight;
		dirLight.Position = XMFLOAT3(5, 20, 20);
		dirLight.Strength = XMFLOAT3(0.6, 0.2, 0.2);
		dirLight.Direction = XMFLOAT3(-1, -1, -1);
		mLights.push_back(dirLight);
	}
}

void Shadow::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)mOpaqueRenderitems.size();
	UINT frameCount = gNumFrameResources;
	UINT shadowMapCount = 1;

	UINT numDescriptors = 
		objCount * gNumFrameResources 
		+ frameCount 
		+ shadowMapCount * gNumFrameResources 
		+ mMaxLightNum * gNumFrameResources // Every light has a shadow map
		+ mMaxLightNum * gNumFrameResources;

	mPassCbvOffset = objCount * gNumFrameResources;
	mShadowMapTexOffset = objCount * gNumFrameResources + frameCount;
	mShadowMapDsvOffset = 1;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap));
}

void Shadow::BuildBuffers()
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
	}

	{
		for (int frameIndex = 0; frameIndex < gNumFrameResources; frameIndex++) {

			int shadowMapTexIndex = mShadowMapTexOffset + frameIndex;

			// for UAV

			//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			//srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			//srvDesc.Texture2D.MostDetailedMip = 0;
			//srvDesc.Texture2D.MipLevels = 1;

			//auto shadowMapTexCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			//shadowMapTexCpuHandle.Offset(shadowMapTexIndex, mCbvSrvUavDescriptorSize);
			//md3dDevice->CreateShaderResourceView(mShadowMap->Output(), &srvDesc, shadowMapTexCpuHandle);


			// for DSV
			auto srvCpuStart = mCbvHeap->GetCPUDescriptorHandleForHeapStart();
			auto srvGpuStart = mCbvHeap->GetGPUDescriptorHandleForHeapStart();
			auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

			mShadowMap->RenderTexture::BuildDescriptors(
				CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapTexOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapTexOffset + frameIndex, mCbvSrvUavDescriptorSize),
				CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, mShadowMapDsvOffset + frameIndex, mDsvDescriptorSize));
		}
	}

	{
		mLightBuffer = std::make_unique<UploadBuffer<Light>>(md3dDevice.Get(), mMaxLightNum, false);
		mLightShadowTransformBuffer = std::make_unique<UploadBuffer<XMFLOAT4X4>>(md3dDevice.Get(), mMaxLightNum, false);
		mShadowMapUseBuffer = std::make_unique<UploadBuffer<ShadowMapUse>>(md3dDevice.Get(), 1, false);
	}
}

void Shadow::CreateRtvAndDsvDescriptorHeaps()
{
	// Common creation.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1 + gNumFrameResources;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void Shadow::BuildPSOs()
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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"]));

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowMapPresentPsoDesc = opaquePsoDesc;
		shadowMapPresentPsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["shadowMapPresentVS"]->GetBufferPointer()),
			mShaders["shadowMapPresentVS"]->GetBufferSize()
		};
		shadowMapPresentPsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["shadowMapPresentPS"]->GetBufferPointer()),
			mShaders["shadowMapPresentPS"]->GetBufferSize()
		};
		md3dDevice->CreateGraphicsPipelineState(&shadowMapPresentPsoDesc, IID_PPV_ARGS(&mPSOs["shadowMapPresent"]));
	}

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowMapPsoDesc;

		ZeroMemory(&shadowMapPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		shadowMapPsoDesc.InputLayout = { mInputLayoutShadowMap.data(), (UINT)mInputLayoutShadowMap.size() };
		shadowMapPsoDesc.pRootSignature = mShadowMapRootSignature.Get();
		shadowMapPsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(mShaders["shadowMapVS"]->GetBufferPointer()),
			mShaders["shadowMapVS"]->GetBufferSize()
		};
		shadowMapPsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(mShaders["shadowMapPS"]->GetBufferPointer()),
			mShaders["shadowMapPS"]->GetBufferSize()
		};

		shadowMapPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		shadowMapPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		shadowMapPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		shadowMapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		shadowMapPsoDesc.SampleMask = UINT_MAX;
		shadowMapPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		shadowMapPsoDesc.NumRenderTargets = 1;
		shadowMapPsoDesc.RTVFormats[0] = mBackBufferFormat;
		shadowMapPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
		shadowMapPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
		shadowMapPsoDesc.DSVFormat = mDepthStencilFormat;
		md3dDevice->CreateGraphicsPipelineState(&shadowMapPsoDesc, IID_PPV_ARGS(&mPSOs["shadowMap"]));
	}
}

void Shadow::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
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

		cmdList->SetGraphicsRootDescriptorTable((UINT)DefaultPSO::perObjectCB, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}


