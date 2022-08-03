cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPerPass : register(b1)
{
    float4x4 gView;
    float4x4 gProj;
    int gLightCount;
    float3 gEyePos;
};


struct ShadowMapUse
{
    float4x4 view;
    float4x4 proj;
};

StructuredBuffer<ShadowMapUse> gUseData : register(t0);

struct Light
{
    float3 Strength;
    float FalloffStart; // point/spot light only
    float3 Direction; // directional/spot light only
    float FalloffEnd; // point/spot light only
    float3 Position; // point/spot light only
    float SpotPower; // spot light only
};

struct Vertex
{
    float3 albedo;
    float3 normal;
    float3 pos;
};

struct VertexIn
{
    float3 posLocal : POSITION;
};

struct VertexOut
{
    float4 posProj : SV_POSITION;
};

#include "../Common/common.hlsl"

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.posProj = mul(float4(vin.posLocal, 1.0f), gWorld);
    vout.posProj = mul(vout.posProj, gUseData[0].view);
    vout.posProj = mul(vout.posProj, gUseData[0].proj);

    return vout;
};

void PS(VertexOut pin)
{
    
};