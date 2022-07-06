#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "MyApp.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

class TessellationApp :public MyApp
{
public:
	struct Vertex
	{
		XMFLOAT3 pos;
	};

	struct ObjectConstants
	{
		XMFLOAT4X4 world = MathHelper::Identity4x4();
		XMFLOAT4X4 viewProj = MathHelper::Identity4x4();
		XMFLOAT3 eyePos = XMFLOAT3(0, 0, 0);
	};

	TessellationApp(HINSTANCE hInstance);
	virtual bool Initialize()override;

	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
private:
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	void BuildDescriptorHeaps();

	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
	void BuildConstantBuffers();

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	void BuildRootSignature();

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;
	ComPtr<ID3DBlob> mhsByteCode = nullptr;
	ComPtr<ID3DBlob> mdsByteCode = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	void BuildShaderAndInputLayout();

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
	void BuildObject();

	ComPtr<ID3D12PipelineState> mPSO = nullptr;
	void BuildPSO();
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
		TessellationApp theApp(hInstance);
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

TessellationApp::TessellationApp(HINSTANCE hInstance) :
	MyApp(hInstance)
{
}

bool TessellationApp::Initialize()
{
	if (!MyApp::Initialize())return false;

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildShaderAndInputLayout();
	BuildObject();
	BuildPSO();

	mCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	return true;
}

void TessellationApp::Update(const GameTimer& gt)
{
	MyApp::Update(gt);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMLoadFloat4x4(&mView) * proj;

	// Update the constant buffer with the latest worldViewProj matrix.
	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.world, XMMatrixTranspose(world));
	XMStoreFloat4x4(&objConstants.viewProj, XMMatrixTranspose(viewProj));
	objConstants.eyePos = mEyePos;
	mObjectCB->CopyData(0, objConstants);
}

void TessellationApp::Draw(const GameTimer& gt)
{
	mDirectCmdListAlloc->Reset();

	mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get());
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

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());


	// ![Tip]: Note that "D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST" replace "D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST"

	mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
	//mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->DrawIndexedInstanced(
		mBoxGeo->DrawArgs["object"].IndexCount,
		1, 0, 0, 0
	);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	mCommandList->Close();

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	mSwapChain->Present(0, 0);
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void TessellationApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	md3dDevice->CreateDescriptorHeap(
		&cbvHeapDesc,
		IID_PPV_ARGS(&mCbvHeap)
	);
}

void TessellationApp::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 3, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	md3dDevice->CreateConstantBufferView(
		&cbvDesc,
		mCbvHeap->GetCPUDescriptorHandleForHeapStart()
	);
}

void TessellationApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
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
}

void TessellationApp::BuildShaderAndInputLayout()
{
	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\Tessellation\\shader.hlsl", nullptr, "VS", "vs_5_0");
	mhsByteCode = d3dUtil::CompileShader(L"Shaders\\Tessellation\\shader.hlsl", nullptr, "HS", "hs_5_0");
	mdsByteCode = d3dUtil::CompileShader(L"Shaders\\Tessellation\\shader.hlsl", nullptr, "DS", "ds_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\Tessellation\\shader.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void TessellationApp::BuildObject()
{
	std::array<Vertex, 4> vertices =
	{
		Vertex({ XMFLOAT3(-10.0f, -10.0f, +10.0f) }),
		Vertex({ XMFLOAT3(+10.0f, -10.0f, +10.0f) }),
		Vertex({ XMFLOAT3(-10.0f, -10.0f, -10.0f) }),
		Vertex({ XMFLOAT3(+10.0f, -10.0f, -10.0f) }),
	};
	std::array<std::uint16_t, 4> indices =
	{
		0,1,2,3
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

void TessellationApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.HS =
	{
		reinterpret_cast<BYTE*>(mhsByteCode->GetBufferPointer()),
		mhsByteCode->GetBufferSize()
	};
	psoDesc.DS =
	{
		reinterpret_cast<BYTE*>(mdsByteCode->GetBufferPointer()),
		mdsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
