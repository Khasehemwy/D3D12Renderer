#include "../../Common/d3dApp.h"
#include "../../Common//Camera.h"
using namespace DirectX;

class MyApp :public D3DApp
{
public:
	MyApp(HINSTANCE hInstance);

	virtual bool Initialize()override;
	virtual void OnResize()override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void OnKeyboardInput(const GameTimer& gt);
	virtual void Update(const GameTimer& gt)override;
	virtual std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();


protected:
	Camera mCamera;

	bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0,0,0 };
	POINT mLastMousePos;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

private:
	static std::wstring GetLatestWinPixGpuCapturerPath_Cpp17();
};