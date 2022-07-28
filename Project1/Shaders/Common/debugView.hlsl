cbuffer cbPerPass : register(b0)
{
    uint gPosId;
};


Texture2D gTex : register(t0);

SamplerState gSamPointWrap : register(s0);
SamplerState gSamPointClamp : register(s1);
SamplerState gSamLinearWrap : register(s2);
SamplerState gSamLinearClamp : register(s3);
SamplerState gSamAnisotropicWrap : register(s4);
SamplerState gSamAnisotropicClamp : register(s5);
SamplerState gSamDepthMap : register(s6);

static const float2 gPos[4][6] =
{
    //bottom0
    { float2(-1.0, -1.0), float2(-1.0, -0.5), float2(-0.5, -0.5), float2(-1.0, -1.0), float2(-0.5, -0.5), float2(-0.5, -1.0) },
    //bottom1
    { float2(-0.5, -1.0), float2(-0.5, -0.5), float2(0.0, -0.5), float2(-0.5, -1.0), float2(0.0, -0.5), float2(0.0, -1.0) },
    //bottom2
    { float2(0.0, -1.0), float2(0.0, -0.5), float2(0.5, -0.5), float2(0.0, -1.0), float2(0.5, -0.5), float2(0.5, -1.0) },
    //bottom3
    { float2(0.5, -1.0), float2(0.5, -0.5), float2(1.0, -0.5), float2(0.5, -1.0), float2(1.0, -0.5), float2(1.0, -1.0) },
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
    //float2(0.75, 1),
    //float2(0.75, 0.75),
    //float2(1, 0.75),
    //float2(0.75, 1),
    //float2(1, 0.75),
    //float2(1, 1)


struct VertexOut
{
    float4 posH : SV_POSITION;
    float2 uv : UV;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    vout.uv = gTexCoords[vid];
    vout.posH = float4(gPos[gPosId][vid], 0, 1);

    return vout;
};

float4 PS(VertexOut pin) : SV_TARGET
{
    return gTex.Sample(gSamPointWrap, pin.uv);
};