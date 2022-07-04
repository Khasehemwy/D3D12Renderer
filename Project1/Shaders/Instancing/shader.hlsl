struct InstanceData
{
    float4x4 World;
    float3 Color;
};

struct VertexIn
{
    float3 pos : POSITION;
};

struct VertexOut
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);

cbuffer cbPerObject : register(b0)
{
    float4x4 gViewProj;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout;

    vout.pos = mul(mul(float4(vin.pos, 1.0f), gInstanceData[instanceID].World), gViewProj);
    vout.color = float4(gInstanceData[instanceID].Color, 1);
    
    //vout.pos = mul(float4(vin.pos, 1.0f), gViewProj);
    //vout.color = float4(1, 0, 0, 1);

    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    return pin.color;
}