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
    float3 normalV : NORMAL_VIEW;
};

struct PixelOut
{
    float4 normal : COLOR0;
    float3 color : COLOR1;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.posProj = mul(float4(vin.posLocal, 1.0f), gWorld);
    vout.posProj = mul(vout.posProj, gView);
    vout.posProj = mul(vout.posProj, gProj);

    //vout.color = float4(0.6, 0.6, 0.6, 1);
    
    float4x4 normalWorldMatrix = transpose(inverse(gWorld));
    vout.normalV = mul(vin.normal, (float3x3) normalWorldMatrix);
    vout.normalV = mul(vin.normal, (float3x3) gWorld);
    vout.normalV = mul(vout.normalV, (float3x3) gView);
    vout.normalV = normalize(vout.normalV);

    return vout;
};

PixelOut PS(VertexOut pin) : SV_TARGET
{
    PixelOut pout;
    pout.normal = float4(pin.normalV, 1.0f);
    pout.color = float4(1, 1, 1, 1);
    return pout;
};