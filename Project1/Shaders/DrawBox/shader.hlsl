cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
};

struct VertexIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VertexOut
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.pos = mul(mul(float4(vin.pos, 1.0f),gWorld),gViewProj);
    vout.color = vin.color;

    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    return pin.color;
}