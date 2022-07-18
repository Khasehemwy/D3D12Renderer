#include "../../Common/d3dUtil.h"

class RenderTexture
{
public:
	RenderTexture(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format);

	RenderTexture(const RenderTexture& rhs) = delete;

	ID3D12Resource* Output();

	void OnResize(UINT newWidth, UINT newHeight);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv,
		DXGI_FORMAT srvFormat = DXGI_FORMAT_R8G8B8A8_UNORM);

	CD3DX12_CPU_DESCRIPTOR_HANDLE Srv()const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv()const;

private:
	void BuildResources();
	void BuildDescriptors();

	ID3D12Device* md3dDevice = nullptr;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mSrvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTex = nullptr;
};