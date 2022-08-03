#include "../Common/common.hlsl"

Texture2D gColor : register(t0);
Texture2D gSsaoMap : register(t1);

SamplerState gSamPointWrap : register(s0);
SamplerState gSamPointClamp : register(s1);
SamplerState gSamLinearWrap : register(s2);
SamplerState gSamLinearClamp : register(s3);
SamplerState gSamAnisotropicWrap : register(s4);
SamplerState gSamAnisotropicClamp : register(s5);
SamplerState gSamDepthMap : register(s6);

static const float2 gPos[6] =
{
    { float2(-1.0, -1.0), float2(-1.0, 1.0), float2(1.0, 1.0), float2(-1.0, -1.0), float2(1.0, 1.0), float2(1.0, -1.0) },
};


static const float2 gTexCoords[6] =
{
    float2(0, 1),
    float2(0, 0),
    float2(1, 0),
    float2(0, 1),
    float2(1, 0),
    float2(1, 1)
};

struct VertexOut
{
    float4 posH : SV_POSITION;
    float2 uv : UV;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    vout.uv = gTexCoords[vid];
    vout.posH = float4(gPos[vid], 0, 1);

    return vout;
};

float4 PS(VertexOut pin) : SV_TARGET
{
    float4 color = float4(gColor.Sample(gSamPointWrap, pin.uv));
    float ssao = gSsaoMap.Sample(gSamPointWrap, pin.uv).r;
    if (ssao < 1.0f)
    {
        color *= ssao;
    }
    return color;
};