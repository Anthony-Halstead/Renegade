[[vk::binding(0, 1)]] Texture2D gAlbedoTextures[];
[[vk::binding(1, 1)]] Texture2D gNormalTextures[];
[[vk::binding(2, 1)]] Texture2D gEmissiveTextures[];
[[vk::binding(3, 1)]] Texture2D gAlphaTextures[];
[[vk::binding(4, 1)]] Texture2D gSpecularTextures[];
[[vk::binding(0, 1)]] SamplerState gSampler;

struct OBJ_ATTRIBUTES
{
    float3 Kd;
    float d;
    float3 Ks;
    float Ns;
    float3 Ka;
    float sharpness;
    float3 Tf;
    float Ni;
    float3 Ke;
    float illum;

    uint albedoIndex;
    uint normalIndex;
    uint emissiveIndex;
    uint alphaIndex;
    uint specularIndex;
};

struct SHADER_MODEL_DATA
{
    float4x4 worldMatrix;
    OBJ_ATTRIBUTES material;
};

StructuredBuffer<SHADER_MODEL_DATA> SceneData : register(b1);

cbuffer SHADER_SCENE_DATA : register(b0)
{
    float4 sunDirection, sunColor, sunAmbient, camPos;
    float4x4 viewMatrix, projectionMatrix;
};

float4 main(
    float4 pos : SV_POSITION,
    float3 nrm : NORMAL,
    float3 posW : WORLD,
    float3 uvw : TEXCOORD,
    uint index : INDEX
) : SV_TARGET
{
    OBJ_ATTRIBUTES mat = SceneData[index].material;

    float4 albedo = gAlbedoTextures[mat.albedoIndex].Sample(gSampler, uvw.xy);
    float4 normalMap = gNormalTextures[mat.normalIndex].Sample(gSampler, uvw.xy);
    float4 emissive = gEmissiveTextures[mat.emissiveIndex].Sample(gSampler, uvw.xy);
    float4 alphaMap = gAlphaTextures[mat.alphaIndex].Sample(gSampler, uvw.xy);
    float4 specular = gSpecularTextures[mat.specularIndex].Sample(gSampler, uvw.xy);

    float3 N = normalize(nrm);

    if (abs(normalMap.r - 0.5) > 0.01 || abs(normalMap.g - 0.5) > 0.01)
    {
        float3 normalTS = normalize(2.0 * normalMap.xyz - 1.0);
        N = normalize(normalTS);
    }
    else
    {
        float bumpStrength = 0.1f;
        float2 texel = float2(1.0 / 1024.0, 1.0 / 1024.0);
        float center = normalMap.r;
        float left = gNormalTextures[mat.normalIndex].Sample(gSampler, uvw.xy + float2(-texel.x, 0)).r;
        float right = gNormalTextures[mat.normalIndex].Sample(gSampler, uvw.xy + float2(texel.x, 0)).r;
        float down = gNormalTextures[mat.normalIndex].Sample(gSampler, uvw.xy + float2(0, -texel.y)).r;
        float up = gNormalTextures[mat.normalIndex].Sample(gSampler, uvw.xy + float2(0, texel.y)).r;
        float3 bumpNormal = normalize(float3((left - right), (down - up), bumpStrength));
        N = normalize(lerp(N, bumpNormal, 0.5));
    }

    float alpha = mat.d * alphaMap.r;

    float3 sunDir = normalize(-sunDirection.xyz);
    float NdotL = max(dot(N, sunDir), 0.0);

    float3 viewDir = normalize(camPos.xyz - posW);
    float3 halfDir = normalize(sunDir + viewDir);

    float specIntensity = pow(max(dot(N, halfDir), 0.0), mat.Ns) * specular.r;

    float4 diffuse = albedo * float4(mat.Kd, 1.0);
    float4 ambient = float4(mat.Ka, 1.0) * sunAmbient;
    float4 emit = emissive * float4(mat.Ke, 1.0);
    float4 spec = float4(mat.Ks * specIntensity, 1.0);

    float4 color = (ambient + sunColor * NdotL * diffuse + spec) + emit;
    color.a = alpha;

    return saturate(color);
}
