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

    vout.pos = float4(vin.pos.xy, 0, 1);
    vout.color = vin.color;
    vout.tex = vin.tex;

    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float4 color;
    float w, h;
    gTex.GetDimensions(w, h);
    float2 tex_offset = float2(1.0 / w, 1.0 / h);

    int totSample = 10;
    color = gTex.Sample(gSampler, pin.tex);
    for (int i = 1; i < totSample; i++) {
        color += gTex.Sample(gSampler, float2(pin.tex.x + i * tex_offset.x, pin.tex.y));
    }
    color /= totSample;
    color.a = 1;
    //color = float4(1, 0, 0, 1);
    //color = pin.color;
    return color;
}