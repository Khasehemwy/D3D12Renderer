#include "../Common/common.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPerPass : register(b1)
{
    float4x4 gView;
    float4x4 gProj;
};

Texture2D gGbuffer[2] : register(t0);
//Gbuffer[0]:normal
//Gbuffer[1]:z

SamplerState gSampler : register(s0);

struct VertexIn
{
    float3 posLocal : POSITION;
    float3 normal : NORMAL;
    float3 tangentU : TANGENTU;
    float2 TexC : TEXC;
};

struct VertexOut
{
    float4 posProj : SV_POSITION;
    float4 color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.posProj = mul(float4(vin.posLocal, 1.0f), gWorld);
    vout.posProj = mul(vout.posProj, gView);
    vout.posProj = mul(vout.posProj, gProj);

    //vout.color = float4(0.6, 0.6, 0.6, 1);
    
    float4x4 normalMatrix = transpose(inverse(gWorld));
    vout.color.rgb = mul(mul(vin.normal, (float3x3) normalMatrix), (float3x3) gWorld);
    vout.color.a = 1;

    return vout;
};

float4 PS(VertexOut pin) : SV_TARGET
{
    return pin.color;
};