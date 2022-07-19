
Texture2D gTex[2] : register(t0);
SamplerState gSampler : register(s0);

struct VertexIn{
    float3 posLocal : POSITION;
    float4 color : COLOR;
};

struct VertexOut{
    float4 posProj : SV_POSITION;
    float2 uv : UV;
};

#include "../Common/common.hlsl"

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.posProj = float4(vin.posLocal, 1.0f);
    vout.uv = float2(vin.color.rg);

    return vout;
};

float4 PS(VertexOut pin): SV_TARGET
{
    float4 color;
    float4 colorGrid0 = gTex[0].Sample(gSampler, pin.uv);
    float4 colorGrid1 = gTex[1].Sample(gSampler, pin.uv);
    if (colorGrid0.a < colorGrid1.a)
    {
        float4 temp = colorGrid0;
        colorGrid0 = colorGrid1;
        colorGrid1 = temp;
    }
    if (colorGrid0.r != 0 && colorGrid0.g != 0 && colorGrid0.b != 0
        && colorGrid1.r != 0 && colorGrid1.g != 0 && colorGrid1.b != 0)
    {
        color = float4(float3(1, 1, 1) - colorGrid0.rgb, 1);
    }
    else
    {
        color = colorGrid0 + colorGrid1;
    }
    return color;
};