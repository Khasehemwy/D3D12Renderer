#define threadBlockSize 128

struct ObjectInfo
{
    float4x4 world;
    float3 boxCenter;
    float3 boxLen;
};

struct IndirectCommand
{
    uint2 cbvAddress;
    uint4 drawArguments;
};

cbuffer RootConstants : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float gCommandCount; // The number of commands to be processed.
};

StructuredBuffer<ObjectInfo> gObjects : register(t0); // SRV: Wrapped constant buffers
StructuredBuffer<IndirectCommand> gInputCommands : register(t1); // SRV: Indirect commands
AppendStructuredBuffer<IndirectCommand> gOutputCommands : register(u0); // UAV: Processed indirect commands

[numthreads(threadBlockSize, 1, 1)]
void CS(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // Each thread of the CS operates on one of the indirect commands.
    uint index = (groupId.x * threadBlockSize) + groupIndex;
    
    if (index < gCommandCount)
    {
        float3 l = gObjects[index].boxLen;
        float3 p = gObjects[index].boxCenter;
        float3 pos[6] =
        {
            float3(p.x + l.x, p.y, p.z),
            float3(p.x - l.x, p.y, p.z),
            float3(p.x, p.y + l.y, p.z),
            float3(p.x, p.y - l.y, p.z),
            float3(p.x, p.y, p.z + l.z),
            float3(p.x, p.y, p.z - l.z)
        };
        
        bool cull = true;
        for (int i = 0; i < 6; i++)
        {
            float4 posH = mul(float4(pos[i], 1.0f), gObjects[index].world);
            posH = mul(posH, gView);
            posH = mul(posH, gProj);
            float w = posH.w;
            if (posH.x >= -w & posH.x <= w && posH.y >= -w && posH.y <= w && posH.z >= 0 && posH.z <= w)
            {
                cull = false;
            }
        }
        if (!cull)
        {
            gOutputCommands.Append(gInputCommands[index]);
        }
    }
}
