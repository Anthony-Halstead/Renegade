// Gateware Research file. Will be used to test adding an asynchronous overlay to GVulkanSurface
// Author: Lari H. Norri 12/31/2024

// HLSL Fragment shader used for overlay rendering

Texture2D oTexture : register(t0);
sampler oSampler : register(s0);

struct PixelIn
{
    float4 Position : SV_POSITION;
};

// push constants for overlay offset and scale
[[vk::push_constant]]
cbuffer OverlayConstants
{
    float2 overlayOffset;
    float2 overlayScale;
};

float4 main(PixelIn input) : SV_TARGET
{
    // offset and scale are inverted CPU side to allow for proper sampling
    return oTexture.SampleLevel( 
        oSampler, (input.Position.xy + overlayOffset) * overlayScale, 0);
}


