Texture2D gGbuffer[3] : register(t0);
Texture2D gSsaoMap : register(t3);
RWTexture2D<float4> gOutput : register(u0);

cbuffer cbPerPass : register(b0)
{
    float4x4 gProj;
    float gBlurRadius;
}

StructuredBuffer<float> gBlurWeights : register(t4);

#define N 256

float NdcDepthToViewDepth(float zNdc)
{
    float zView = gProj[3][2] / (zNdc - gProj[2][2]); //把矩阵相乘的结果进行推导即可得到该等式
    return zView;
}

[numthreads(N, 1, 1)]
void CS(int3 groupThreadID : SV_GroupThreadID,
	int3 dispatchThreadID : SV_DispatchThreadID)
{
	int dispatch_x = dispatchThreadID.x;
	int dispatch_y = dispatchThreadID.y;
    float4 color = float4(0, 0, 0, 1);

	int x = groupThreadID.x;
	int y = groupThreadID.y;

    float3 centerNormal = gGbuffer[0][int2(dispatch_x, dispatch_y)].xyz;
    float centerDepth = NdcDepthToViewDepth(gGbuffer[1][int2(dispatch_x, dispatch_y)].r);
    
    if (gGbuffer[1][int2(dispatch_x, dispatch_y)].r < 1.0f)
    {   
        float totWeight = 0;
        for (int i = -gBlurRadius; i < gBlurRadius; i++)
        {
            for (int j = -gBlurRadius; j < gBlurRadius; j++)
            {
                if (dispatch_x + i <= gSsaoMap.Length.x - 1 && dispatch_x + i > 0
                    && dispatch_y + j <= gSsaoMap.Length.y - 1 && dispatch_y + j > 0)
                {
                    int2 tex = int2(dispatch_x + i, dispatch_y + j);
            
                    float3 neighborNormal = gGbuffer[0][tex].xyz;
                    float neighborDepth = NdcDepthToViewDepth(gGbuffer[1][tex].r);
            
                    if (dot(neighborNormal, centerNormal) >= 0.8f &&
		                abs(neighborDepth - centerDepth) <= 0.2f)
                    {
                        float weight = gBlurWeights[i + gBlurRadius];
                        color += gSsaoMap[tex] * weight;
                        totWeight += weight;
                    }
                }               
            }
	    }

        color /= totWeight;
    }
	
    if (gGbuffer[1][int2(dispatch_x, dispatch_y)].r == 1.0f)
    {
        color = gSsaoMap[int2(dispatch_x, dispatch_y)];
    }
    
	gOutput[int2(dispatch_x, dispatch_y)] = color;
}