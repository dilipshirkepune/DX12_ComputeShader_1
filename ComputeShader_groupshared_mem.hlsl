// Input texture (read-only)
Texture2D<float> inputTexture : register(t0);

// Output buffer (read-write)
RWStructuredBuffer<float> outputBuffer : register(u0);

// Define the number of threads per group
[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 GID : SV_GroupID)
{
    // Shared memory for reduction
    groupshared float sharedData[16 * 16];

    // Calculate the index based on the thread ID
    uint index = DTid.y * 16 + DTid.x;

    // Read the value from the input texture
    float value = inputTexture.Load(int3(DTid.xy, 0));

    // Store the value in shared memory
    sharedData[GTid.y * 16 + GTid.x] = value;

    // Synchronize threads in the group
    GroupMemoryBarrierWithGroupSync();

    // Perform reduction to find the maximum value in the group
    for (uint stride = 8; stride > 0; stride >>= 1)
    {
        if (GTid.x < stride)
        {
            sharedData[GTid.y * 16 + GTid.x] = max(sharedData[GTid.y * 16 + GTid.x], sharedData[GTid.y * 16 + GTid.x + stride]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // Write the maximum value to the output buffer
    if (GTid.x == 0)
    {
        outputBuffer[GID.y * (16) + GID.x] = sharedData[GTid.y * 16];
    }
}