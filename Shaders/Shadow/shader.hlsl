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

    vout.posProj = mul(float4(vin.posLocal,1.0f),gWorld);
    vout.posProj = mul(vout.posProj,gView);
    vout.posProj = mul(vout.posProj,gProj);
    
    vout.posWorld = mul(float4(vin.posLocal,1.0f),gWorld).xyz;
    
    float4x4 normalMatrix = transpose(inverse(gWorld));
    vout.normal = mul(mul(vin.normal, (float3x3)normalMatrix), (float3x3)gWorld);
    vout.normal = normalize(vout.normal);
    
    vout.color = vin.color;

    return vout;
};

float CalcShadow(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    float bias = 0.0005f;
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += (depth - bias < gShadowMap.Sample(gSampler, shadowPosH.xy + offsets[i]).r) ? 1 : 0;
    }
    
    return percentLit / 9.0f;
}

float4 CalculateLight(Vertex v, Light light)
{
    float shadowFactor = CalcShadow(v.posOnLight);
    
    float3 ambient = v.albedo * float3(0.2, 0.2, 0.2);
    
    if (shadowFactor < 1.0f)
    {
        return float4(ambient + ambient * shadowFactor, 1.0f);
    }
    
    float3 normal = normalize(v.normal);
    float3 lightDir = normalize(-light.Direction);
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = light.Strength * diff * v.albedo;
    
    float3 reflectDir = normalize(reflect(-lightDir,normal));
    float3 viewDir = normalize(gEyePos - v.pos);
    float shininess = 32.0f;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    float3 specular = float3(1, 1, 1) * spec * v.albedo;
    
    return float4(ambient + diffuse + specular, 1.0f);
}

float4 PS(VertexOut pin): SV_TARGET
{
    float4 color = float4(0, 0, 0, 1);
    for (int i = 0; i < gLightCount; i++)
    {
        Vertex v;
        v.albedo = pin.color;
        v.normal = pin.normal;
        v.pos = pin.posWorld;
        v.posOnLight = mul(float4(pin.posWorld, 1), gLightShadowTransform[i]);
        
        color += CalculateLight(v, gLights[i]);        
    }
    return color;
};