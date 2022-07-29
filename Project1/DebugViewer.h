#pragma once
#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"

class DebugViewer
{
public:
	enum class Position : int
	{
		Bottom0,
		Bottom1,
		Bottom2,
		Bottom3,
	};

	DebugViewer(
		Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
		DXGI_FORMAT rtvFormat,
		UINT cbvSrvUavDescriptorSize,
		int numFrame);

	void Draw(D3D12_CPU_DESCRIPTOR_HANDLE rtv, UINT frameIndex);

	void SetTexSrv(Microsoft::WRL::ComPtr<ID3D12Resource> tex, DXGI_FORMAT format);

	void SetPosition(DebugViewer::Position pos = DebugViewer::Position::Bottom3);

private:
	struct PerPassCB {
		UINT posId;
	};
	std::vector<std::unique_ptr<UploadBuffer<PerPassCB>>> mPassCBs;
	PerPassCB mPassData;

	UINT mCbvSrvUavDescriptorSize = 0;

	int mNumFrame = 1;

	bool m4xMsaaState = false;
	UINT m4xMsaaQuality = 0;

	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	int mTexOffset = 0;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDescriptorHeap = nullptr;

	DXGI_FORMAT mRtvFormat;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;

	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildBuffer();
	void BuildPSO();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
};