// Simple Compute Shader to find max texel value within input texture
// FInd max texel value using parallel reduction implementation. 
// Each thread compare values in shared and reduce by half of 
// thread group size until reach to single max
// 
// Input - R8_UNORM/uint inputTexture
// Output - uint - Max values found by each thread 
// 
// API - DX12 12_0
// Shader Model - cs_5_1
// 
// cmdline to generate .cso - fxc /T cs_5_1 /Fo ComputeShader16x16x1.cso /E CSMain CompuetShader.hlsl
// 
// TODO : Is it safe to use global shared data? Any alternative
// TODO : Instead of THREAD_GROUP_SIZE defined can we use 3 differnt entry point to avoid cso generation and usage error issues

// thread group size - change 8/16/32 before generating .cso 
#define THREAD_GROUP_SIZE 32

// input texture
Texture2D<uint> inputTexture : register(t0);

// output buffer
RWStructuredBuffer<uint> outputBuffer : register(u0);

// shared global - warnings generated as we writing to global storage - not recommend for shaders
// ToDo - check for alternative solution
groupshared float sharedData[THREAD_GROUP_SIZE * THREAD_GROUP_SIZE];

// thread group size x, y, z=1
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 GID : SV_GroupID)
{
    // get the index
    uint index = GTid.y * THREAD_GROUP_SIZE + GTid.x;

    //input texture format R8_UNORM so just red channel to be considered.
    float value = inputTexture.Load(int3(DTid.xy, 0)).r / 255.0f;

    //write to global storage
    sharedData[index] = value;

    //sync before getting into the max value search, to ensure thread writes to shared mem
    GroupMemoryBarrierWithGroupSync();

    // reduction method used to share work load between threads
    for (uint stride = THREAD_GROUP_SIZE / 2; stride > 0; stride >>= 1)
    {
        if (GTid.x < stride)
        {
            sharedData[index] = max(sharedData[index], sharedData[index + stride]);
        }
        GroupMemoryBarrierWithGroupSync(); // ensure sync
    }

    // write max to output once reduction complete
    if (GTid.x == 0 && GTid.y == 0)
    {
        outputBuffer[GID.y * (THREAD_GROUP_SIZE) + GID.x] = (uint) (sharedData[0] * 255.0f);
    }
}