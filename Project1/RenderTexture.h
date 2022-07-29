#include "../../Common/d3dUtil.h"

class RenderTexture
{
public:
	RenderTexture(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	RenderTexture(const RenderTexture& rhs) = delete;

	ID3D12Resource* Output();

	void OnResize(UINT newWidth, UINT newHeight);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);


	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv()const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv()const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv()const;

	D3D12_VIEWPORT Viewport()const;
	D3D12_RECT ScissorRect()const;

	DXGI_FORMAT Format()const;
	DXGI_FORMAT DsvFormat()const;
	DXGI_FORMAT SrvFormat()const;
	D3D12_RESOURCE_FLAGS Flag()const;

protected:
	virtual void BuildDescriptors() = 0;

	ID3D12Device* md3dDevice = nullptr;

	UINT mWidth = 0;
	UINT mHeight = 0;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGI_FORMAT mSrvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_RESOURCE_FLAGS mFlag;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTex = nullptr;
private:
	void BuildResources();
};