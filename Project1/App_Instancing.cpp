#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "MyApp.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;


class InstancingApp :public MyApp
{
public:
	struct InstanceData
	{
		XMFLOAT4X4 World;
		XMFLOAT3 Color;
	};

	struct Vertex
	{
		XMFLOAT3 pos;
	};

	struct ObjectConstants
	{
		XMFLOAT4X4 viewProj = MathHelper::Identity4x4();
	};

	InstancingApp(HINSTANCE hInstance);
	virtual bool Initialize()override;

	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

private:
	const int mMaxInstanceNum = 8 * 10 * 10 * 10;

	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	void BuildDescriptorHeaps();

	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<InstanceData>> mInstanceBuffer = nullptr;
	void BuildBuffers();

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	void BuildRootSignature();

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	void BuildShaderAndInputLayout();

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
	std::vector<InstanceData>mInstanceData;
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
		InstancingApp theApp(hInstance);
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

InstancingApp::InstancingApp(HINSTANCE hInstance) :
	MyApp(hInstance)
{
}

bool InstancingApp::Initialize()
{
	if (!MyApp::Initialize())return false;

	mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr);

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildBuffers();
	BuildShaderAndInputLayout();
	BuildObject();
	BuildPSO();

	mCommandList->Close();
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	return true;
}

void InstancingApp::Update(const GameTimer& gt)
{
	MyApp::Update(gt);

	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMLoadFloat4x4(&mView) * proj;

	// Update the constant buffer with the latest worldViewProj matrix.
	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.viewProj, XMMatrixTranspose(viewProj));
	mObjectCB->CopyData(0, objConstants);

	for (int i = 0; i < mInstanceData.size(); i++) {
		mInstanceBuffer->CopyData(i, mInstanceData[i]);
	}
}

void InstancingApp::Draw(const GameTimer& gt)
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

	mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto instanceBuffer = mInstanceBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(1, instanceBuffer->GetGPUVirtualAddress());

	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->DrawIndexedInstanced(
		mBoxGeo->DrawArgs["object"].IndexCount,
		mInstanceData.size(), 0, 0, 0
	);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void InstancingApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta -= dx;
		mPhi -= dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 1.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 1.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		//mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void InstancingApp::BuildDescriptorHeaps()
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

void InstancingApp::BuildBuffers()
{
	{
		mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

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

	{
		mInstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(md3dDevice.Get(), mMaxInstanceNum, false);
	}
}

void InstancingApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	slotRootParameter[1].InitAsShaderResourceView(0, 1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
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

void InstancingApp::BuildShaderAndInputLayout()
{
	HRESULT hr = S_OK;

	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\Instancing\\shader.hlsl", nullptr, "VS", "vs_5_1");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\Instancing\\shader.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void InstancingApp::BuildObject()
{
	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f) }),
	};
	std::array<std::uint16_t, 36> indices =
	{
		2,1,0,2,3,1,//bottom
		4,5,6,5,7,6,//top
		6,7,2,7,3,2,//front
		0,5,4,0,1,5,//back
		4,6,0,6,2,0,//left
		7,5,3,5,1,3//right
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

	{
		float r = 0;
		int len = std::cbrt(mMaxInstanceNum / 8);
		int step = 20;
		for (int x = -len; x < len; x++) {
			for (int y = -len; y < len; y++) {
				for (int z = -len; z < len; z++) {
					XMMATRIX worldMatrix = XMMatrixIdentity();
					worldMatrix = worldMatrix * XMMatrixTranslation(x * step, y * step, z * step);
					XMFLOAT4X4 world;
					XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
					XMFLOAT3 color(r, 0, 0); r += 0.0001;
					mInstanceData.push_back({ world, color });
				}
			}
		}
	}
}

void InstancingApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS = {
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
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

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
