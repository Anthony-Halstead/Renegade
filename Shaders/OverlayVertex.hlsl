// Gateware Research file. Will be used to test adding an asynchronous overlay to GVulkanSurface
// Author: Lari H. Norri 12/31/2024

// HLSL vertex shader used for overlay rendering

// Output vertex format
struct VertexOut
{
    float4 Position : SV_POSITION; // Don't always trust Ai output, it forgot this and I lost hours!
};

const static VertexOut quad[4] =
{
    { float4(-1.0f, -1.0f, 0.0f, 1.0f) },
    { float4(1.0f, -1.0f, 0.0f, 1.0f) },
    { float4(-1.0f, 1.0f, 0.0f, 1.0f) },
    { float4(1.0f, 1.0f, 0.0f, 1.0f) }
};

// Main vertex shader
VertexOut main(uint id : SV_VertexID)
{
    VertexOut output = quad[id];
    return output;
}
