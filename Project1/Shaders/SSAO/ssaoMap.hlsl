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
    float zView = gProj[3][2] / (zNdc - gProj[2][2]); //�Ѿ�����˵Ľ�������Ƶ����ɵõ��õ�ʽ
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
    vout.posVNear = ph / ph.w; //������任���۲�ռ�Ľ�ƽ��
    
    return vout;
};

float PS(VertexOut pin) : SV_TARGET
{
    float3 n = gGbuffer[0].SampleLevel(gSamPointClamp, pin.texC, 0.0f).xyz;
    float pz = gGbuffer[1].SampleLevel(gSamDepthMap, pin.texC, 0.0f).r;
    
    //��ȡ�����ض�Ӧ�ڹ۲�ռ��еĵ�
    pz = NdcDepthToViewDepth(pz);
    float3 p = ((pz / pin.posVNear.z) * pin.posVNear).xyz;
    
    //��ȡ�����������[0,1]ӳ����[-1,1]
    float3 randVec = 2.0f * gRandomVectorMap.SampleLevel(gSamLinearWrap, 4 * pin.texC, 0.0f).rgb - 1.0f;
    
    float occlusionSum = 0.0f;
    
    for (int i = 0; i < gNumOffsetVec; i++)
    {
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
        
        float flip = sign(dot(offset, n));
        
        float3 q = p + flip * gOcclusionRadius * offset;
        
        //����qͶӰ��NDC
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