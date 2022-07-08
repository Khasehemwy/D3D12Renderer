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

StructuredBuffer<Light> gLights : register(t0, space1);

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
    vout.posProj = mul(vout.posProj, gView);
    vout.posProj = mul(vout.posProj, gProj);

    return vout;
};

float4 PS(VertexOut pin) : SV_TARGET
{
    return pin.posProj.z;
};