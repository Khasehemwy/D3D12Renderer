Texture2D    gTex : register(t0);
RWTexture2D<float4> gOutput : register(u0);

#define N 256

[numthreads(N, 1, 1)]
void CS(int3 groupThreadID : SV_GroupThreadID,
	int3 dispatchThreadID : SV_DispatchThreadID)
{
	int dispatch_x = dispatchThreadID.x;
	int dispatch_y = dispatchThreadID.y;
	float4 color = gTex[int2(dispatch_x, dispatch_y)];

	int x = groupThreadID.x;
	int y = groupThreadID.y;

	int blurNum = 10;
	int totBlur = 1;
	for (int i = 1; i < blurNum; i++) {
		if (x + i >= N)continue;
		totBlur++;
		color += gTex[int2(dispatch_x + i, dispatch_y)];
	}
	color /= totBlur;

	gOutput[int2(dispatch_x, dispatch_y)] = color;
}