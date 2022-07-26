cbuffer cbPerObject : register(b0){
    float4x4 gWorld;
};

cbuffer cbPerPass : register(b1){
    float4x4 gView;
    float4x4 gProj;
};

struct VertexIn{
    float3 posLocal : POSITION;
    float3 normal : NORMAL;
    float3 tangentU : TANGENTU;
    float2 TexC : TEXC;
};

struct VertexOut{
    float4 posProj : SV_POSITION;
    float4 color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.posProj = mul(float4(vin.posLocal,1.0f),gWorld);
    vout.posProj = mul(vout.posProj,gView);
    vout.posProj = mul(vout.posProj,gProj);

    vout.color = mul(float4(vin.normal, 1), gWorld);

    return vout;
};

float4 PS(VertexOut pin): SV_TARGET
{
    return pin.color;
};