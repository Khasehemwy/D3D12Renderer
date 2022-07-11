cbuffer cbPerObject : register(b0){
    float4x4 gWorld;
};

cbuffer cbPerPass : register(b1){
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
    float4 posOnLight;
};

StructuredBuffer<Light> gLights : register(t0, space1);
StructuredBuffer<float4x4> gLightShadowTransform : register(t0, space2);

Texture2D gShadowMap : register(t1);
SamplerState gSampler : register(s0);

struct VertexIn{
    float3 posLocal : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};

struct VertexOut{
    float4 posProj : SV_POSITION;
    float3 posWorld : WORLD_POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};

#include "../Common/common.hlsl"

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.posProj = float4(vin.posLocal, 1.0f);
    vout.normal = vin.normal;
    
    vout.color = vin.color;

    return vout;
};

float4 PS(VertexOut pin): SV_TARGET
{
    return gShadowMap.Sample(gSampler, pin.normal.xy);// use normal as tex coord
};