Texture2D    gTex : register(t0);
SamplerState gSampler  : register(s0);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gViewProj;
};

struct VertexIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
    float2 tex : TEXCOORD;
};

struct VertexOut
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 tex : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.pos = mul(mul(float4(vin.pos, 1.0f),gWorld),gViewProj);
    vout.color = vin.color;
    vout.tex = vin.tex;

    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float4 color;
    color = gTex.Sample(gSampler, pin.tex);
    color.a = 1;
    //color = float4(1, 0, 0, 1);
    return color;
}