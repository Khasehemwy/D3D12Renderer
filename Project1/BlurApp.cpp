#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "RenderTexture.h"
#include "MyApp.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

class BlurApp :public MyApp
{
public:
	struct Vertex
	{
		XMFLOAT3 pos;
		XMFLOAT4 color;
		XMFLOAT2 TexC;
	};

	struct ObjectConstants
	{
		XMFLOAT4X4 world = MathHelper::Identity4x4();
		XMFLOAT4X4 viewProj = MathHelper::Identity4x4();
	};

	BlurApp(HINSTANCE hInstance);
	virtual bool Initialize()override;

	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnResize()override;
private:
	void DrawBlurToRenderTex(const GameTimer& gt, ID3D12Resource* input);

	std::unique_ptr<RenderTexture> renderTex = nullptr;
	std::unique_ptr<RenderTexture> renderTexOut = nullptr;

	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	void LoadTextures();

	ComPtr<ID3D12DescriptorHeap> mSrvUavDescriptorHeap = nullptr;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlurGpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlurGpuUav;
	void BuildDescriptorHeaps();

	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	void BuildBuffers();

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignatureBlur = nullptr;
	void BuildRootSignature();

	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	void BuildShaderAndInputLayout();

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
	void BuildObject();

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	void BuildPSOs();
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
		BlurApp theApp(hInstance);
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

BlurApp::BlurApp(HINSTANCE hInstance) :
	MyApp(hInstance)
{
}

bool BlurApp::Initialize()
{
	if (!D3DApp::Initialize())return false;

	renderTex = std::make_unique<RenderTexture>(
		md3dDevice.Get(),
		mClientWidth, mClientHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM);
	renderTexOut = std::make_unique<RenderTexture>(
		md3dDevice.Get(),
		mClientWidth, mClientHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM);

	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildBuffers();
	BuildShaderAndInputLayout();
	BuildObject();
	BuildPSOs();

	mCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	return true;
}

void BlurApp::Update(const GameTimer& gt)
{
	MyApp::Update(gt);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMLoadFloat4x4(&mView) * proj;

	// Update the constant buffer with the latest worldViewProj matrix.
	auto currObjectCB = ObjectCB.get();
	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.world, XMMatrixTranspose(world));
	XMStoreFloat4x4(&objConstants.viewProj, XMMatrixTranspose(viewProj));
	currObjectCB->CopyData(0, objConstants);
}

void BlurApp::Draw(const GameTimer& gt)
{
	mDirectCmdListAlloc->Reset();

	mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs["default"].Get());
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	));

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvUavDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto objectCB = ObjectCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	mCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress);
	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	mCommandList->SetGraphicsRootDescriptorTable(0, tex);

	mCommandList->DrawIndexedInstanced(
		mBoxGeo->DrawArgs["object"].IndexCount,
		1, 0, 0, 0
	);


	DrawBlurToRenderTex(gt, CurrentBackBuffer());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST));
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTexOut->Output(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE));

	mCommandList->CopyResource(CurrentBackBuffer(), renderTexOut->Output());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTexOut->Output(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ));
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

	mCommandList->Close();

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	mSwapChain->Present(0, 0);
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void BlurApp::OnResize()
{
	MyApp::OnResize();

	if (renderTex != nullptr) {
		renderTex->OnResize(mClientWidth, mClientHeight);
		renderTexOut->OnResize(mClientWidth, mClientHeight);
	}
}

void BlurApp::DrawBlurToRenderTex(const GameTimer& gt, ID3D12Resource* input)
{
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		input,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_COPY_SOURCE
	));
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		renderTex->Output(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_COPY_DEST
	));

	mCommandList->CopyResource(renderTex->Output(), input);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		input,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		renderTex->Output(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_GENERIC_READ
	));
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		renderTexOut->Output(),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	));

	//render use compute shader
	mCommandList->SetPipelineState(mPSOs["blur"].Get());
	mCommandList->SetComputeRootSignature(mRootSignatureBlur.Get());
	mCommandList->SetComputeRootDescriptorTable(0, mBlurGpuSrv);
	mCommandList->SetComputeRootDescriptorTable(1, mBlurGpuUav);

	UINT numGroupX = (UINT)ceilf(mClientWidth / 256.0f);
	mCommandList->Dispatch(numGroupX, mClientHeight, 1);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTexOut->Output(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void BlurApp::LoadTextures()
{
	auto tex = std::make_unique<Texture>();
	tex->Name = "teapot512";
	tex->Filename = L"E:\\Projects\\Project1\\Project1\\Data\\teapot512.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), tex->Filename.c_str(),
		tex->Resource, tex->UploadHeap));

	mTextures[tex->Name] = std::move(tex);
}

void BlurApp::BuildDescriptorHeaps()
{
	{
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1 + 2;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvUavDescriptorHeap));


		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		auto tex = mTextures["teapot512"]->Resource;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);
	}

	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE mBlurCpuSrv
			= CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbvSrvUavDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE mBlurCpuUav
			= CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 2, mCbvSrvUavDescriptorSize);

		mBlurGpuSrv
			= CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, mCbvSrvUavDescriptorSize);
		mBlurGpuUav
			= CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 2, mCbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		md3dDevice->CreateShaderResourceView(renderTex->Output(), &srvDesc, mBlurCpuSrv);
		md3dDevice->CreateUnorderedAccessView(renderTexOut->Output(), nullptr, &uavDesc, mBlurCpuUav);
	}
}

void BlurApp::BuildBuffers()
{
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, 1);
}

void BlurApp::BuildRootSignature()
{
	{
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[1].InitAsConstantBufferView(0);

		auto staticSamplers = GetStaticSamplers();

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			sizeof(slotRootParameter) / sizeof(CD3DX12_ROOT_PARAMETER),
			slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(&mRootSignature)
		);
	};


	//blur
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable;
		srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		CD3DX12_DESCRIPTOR_RANGE uavTable;
		uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &srvTable);
		slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			sizeof(slotRootParameter) / sizeof(CD3DX12_ROOT_PARAMETER),
			slotRootParameter,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignatureBlur.GetAddressOf())
		);
	}
}

void BlurApp::BuildShaderAndInputLayout()
{
	HRESULT hr = S_OK;

	mShaders["defaultVS"] = d3dUtil::LoadBinary(L"Shaders\\Blur\\shader_vs.cso");
	mShaders["defaultPS"] = d3dUtil::LoadBinary(L"Shaders\\Blur\\shader_ps.cso");
	mShaders["blurVS"] = d3dUtil::LoadBinary(L"Shaders\\Blur\\blur_vs.cso");
	mShaders["blurPS"] = d3dUtil::LoadBinary(L"Shaders\\Blur\\blur_ps.cso");

	mShaders["blurCS"] = d3dUtil::LoadBinary(L"Shaders\\Blur\\blur_cs.cso");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 3 * 4, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 3 * 4 + 4 * 4, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void BlurApp::BuildObject()
{
	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Green), XMFLOAT2(0,1)}),//left bottom
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Green), XMFLOAT2(1,1)}),//right bottom
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Green), XMFLOAT2(1,0)}),//right top
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Green), XMFLOAT2(0,0)}),//left top
	};
	std::array<std::uint16_t, 6> indices =
	{
		0,3,2,
		2,1,0
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<MeshGeometry>();
	mBoxGeo->Name = "objectGeo";
	D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU);
	CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU);
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);
	mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

	mBoxGeo->VertexByteStride = sizeof(Vertex);
	mBoxGeo->VertexBufferByteSize = vbByteSize;
	mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mBoxGeo->DrawArgs["object"] = submesh;
}

void BlurApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["defaultVS"]->GetBufferPointer()),
		mShaders["defaultVS"]->GetBufferSize()
	};
	psoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["defaultPS"]->GetBufferPointer()),
		mShaders["defaultPS"]->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;

	md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs["default"]));


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
