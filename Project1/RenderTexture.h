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

private:
	void BuildResources();

	ID3D12Device* md3dDevice = nullptr;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTex = nullptr;
};