#include "../Common/common.hlsl"

cbuffer cbPerPass : register(b1)
{
    float4x4 gProj;
    float4x4 gProjTex;
    float4x4 gInvProj;
    float4 gOffsetVectors[14];
    float gOcclusionRadius;
    float gSurfaceEpsilon;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
};

Texture2D gGbuffer[2] : register(t0);
Texture2D gRandomVectorMap : register(t2);
//Gbuffer[0]:normal
//Gbuffer[1]:z

SamplerState gSamPointWrap : register(s0);
SamplerState gSamPointClamp : register(s1);
SamplerState gSamLinearWrap : register(s2);
SamplerState gSamLinearClamp : register(s3);
SamplerState gSamAnisotropicWrap : register(s4);
SamplerState gSamAnisotropicClamp : register(s5);
SamplerState gSamDepthMap : register(s6);

static const int gNumOffsetVec = 14;

static const float2 gTexCoords[6] =
{
    float2(0, 1),
    float2(0, 0),
    float2(1, 0),
    float2(0, 1),
    float2(1, 0),
    float2(1, 1)
};

struct VertexOut
{
    float4 posH : SV_POSITION;
    float4 posVNear : POSITION_VIEW;
    float2 texC : TEXC;
};

float NdcDepthToViewDepth(float zNdc)
{
    float zView = gProj[3][2] / (zNdc - gProj[2][2]); //把矩阵相乘的结果进行推导即可得到该等式
    return zView;
}

float OcclusionFunction(float disZ)
{
    float occlusion = 0.0f;
    if (disZ > gSurfaceEpsilon)
    {
        float fadeLen = gOcclusionFadeEnd - gOcclusionFadeStart;
        occlusion = saturate((gOcclusionFadeEnd - disZ) / fadeLen);
    }
    return occlusion;
}

VertexOut
    VS(
    uint vid : SV_VertexID)
{
    VertexOut vout;
    
    vout.texC = gTexCoords[vid];
    
    vout.posH = float4(2.0f * vout.texC.x - 1.0f, 1.0f - 2.0f * vout.texC.y, 0, 1);
    
    float4 ph = mul(vout.posH, gInvProj);
    vout.posVNear = ph / ph.w; //将坐标变换到观察空间的近平面
    
    return vout;
};

float PS(VertexOut pin) : SV_TARGET
{
    float3 n = gGbuffer[0].SampleLevel(gSamPointClamp, pin.texC, 0.0f).xyz;
    float pz = gGbuffer[1].SampleLevel(gSamDepthMap, pin.texC, 0.0f).r;
    
    //获取该像素对应在观察空间中的点
    pz = NdcDepthToViewDepth(pz);
    float3 p = ((pz / pin.posVNear.z) * pin.posVNear).xyz;
    
    //提取随机向量并从[0,1]映射至[-1,1]
    float3 randVec = 2.0f * gRandomVectorMap.SampleLevel(gSamLinearWrap, 4 * pin.texC, 0.0f).rgb - 1.0f;
    
    float occlusionSum = 0.0f;
    
    for (int i = 0; i < gNumOffsetVec; i++)
    {
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
        
        float flip = sign(dot(offset, n));
        
        float3 q = p + flip * gOcclusionRadius * offset;
        
        //将点q投影到NDC
        float4 projQ = mul(float4(p, 1.0f), gProjTex);
        projQ /= projQ.w;
        
        float rz = gGbuffer[1].SampleLevel(gSamDepthMap, projQ.xy, 0.0f).r;
        rz = NdcDepthToViewDepth(rz);
        
        float3 r = (rz / q.z) * q;
        
        float disZ = p.z - r.z;
        float dp = max(dot(n, normalize(r - p)), 0.0f);
        float occlusion = dp * OcclusionFunction(disZ);
        
        occlusionSum += occlusion;
    }
    
    occlusionSum /= gNumOffsetVec;
    float access = 1.0f - occlusionSum;
    
    return saturate(pow(access, 2));
};