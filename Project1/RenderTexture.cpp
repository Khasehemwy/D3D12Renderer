#include "RenderTexture.h"

RenderTexture::RenderTexture(
	ID3D12Device* device, 
	UINT width, 
	UINT height, 
	DXGI_FORMAT format,
	D3D12_RESOURCE_FLAGS flag)
{
	md3dDevice = device;
	mWidth = width;
	mHeight = height;
	mFormat = format;
	mSrvFormat = format;
	mFlag = flag;

	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	BuildResources();
}

ID3D12Resource* RenderTexture::Output()
{
	return mRenderTex.Get();
}

void RenderTexture::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResources();
		BuildDescriptors();
	}
}

void RenderTexture::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, 
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, 
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	// Save references to the descriptors. 
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	mhCpuDsv = hCpuDsv;

	//  Create the descriptors
	BuildDescriptors();
}

void RenderTexture::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv)
{
	// Save references to the descriptors. 
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	mhCpuDsv = hCpuDsv;
	mhCpuRtv = hCpuRtv;

	//  Create the descriptors
	BuildDescriptors();
}

void RenderTexture::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuUav)
{
	// Save references to the descriptors. 
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	mhCpuDsv = hCpuDsv;
	mhCpuRtv = hCpuRtv;
	mhCpuUav = hCpuUav;

	//  Create the descriptors
	BuildDescriptors();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE RenderTexture::Srv() const
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE RenderTexture::Dsv() const
{
	return mhCpuDsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE RenderTexture::Rtv() const
{
	return mhCpuRtv;
}

D3D12_VIEWPORT RenderTexture::Viewport() const
{
	return mViewport;
}

D3D12_RECT RenderTexture::ScissorRect() const
{
	return mScissorRect;
}

DXGI_FORMAT RenderTexture::Format() const
{
	return mFormat;
}

DXGI_FORMAT RenderTexture::DsvFormat() const
{
	return mDsvFormat;
}

DXGI_FORMAT RenderTexture::SrvFormat() const
{
	return mSrvFormat;
}

D3D12_RESOURCE_FLAGS RenderTexture::Flag() const
{
	return mFlag;
}

void RenderTexture::BuildResources()
{
	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = mFlag;

	if (texDesc.Flags == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
		//as DSV
		D3D12_CLEAR_VALUE optClear;
		optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(&mRenderTex)
		));
	}

	else if (texDesc.Flags == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
		// as RTV
		D3D12_CLEAR_VALUE clearValue = { mFormat, {} };
		memcpy(clearValue.Color, DirectX::Colors::Black, sizeof(clearValue.Color));

		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&clearValue,
			IID_PPV_ARGS(&mRenderTex)
		));
	}

	else {
		// as UAV
		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&mRenderTex)
		));
	}
}
