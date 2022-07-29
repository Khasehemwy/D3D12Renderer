Texture2D gGbuffer[2] : register(t0);
Texture2D gSsaoMap : register(t2);
RWTexture2D<float4> gOutput : register(u0);

#define N 256

[numthreads(N, 1, 1)]
void CS(int3 groupThreadID : SV_GroupThreadID,
	int3 dispatchThreadID : SV_DispatchThreadID)
{
	int dispatch_x = dispatchThreadID.x;
	int dispatch_y = dispatchThreadID.y;
	//float4 color = gTex[int2(dispatch_x, dispatch_y)];
    float4 color = float4(0, 0, 0, 1);

	int x = groupThreadID.x;
	int y = groupThreadID.y;

	int blurNum = 10;
	int totBlur = 0;
	for (int i = -blurNum; i < blurNum; i++) {
		//if (x + i >= N || x + i < 0)continue;
        if (dispatch_x + i <= gSsaoMap.Length.x - 1 && dispatch_x + i > 0)
        {
			totBlur++;
            color += gSsaoMap[int2(dispatch_x + i, dispatch_y)];
        }
	}
	color /= totBlur;

	gOutput[int2(dispatch_x, dispatch_y)] = color;
}