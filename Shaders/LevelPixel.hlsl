
struct OBJ_ATTRIBUTES
{
	float3      Kd; // diffuse reflectivity
	float	    d; // dissolve (transparency) 
	float3      Ks; // specular reflectivity
	float       Ns; // specular exponent
	float3      Ka; // ambient reflectivity
	float       sharpness; // local reflection map sharpness
	float3      Tf; // transmission filter
	float       Ni; // optical density (index of refraction)
	float3      Ke; // emissive reflectivity
	float		illum; // illumination model
};

struct SHADER_MODEL_DATA
{
    float4x4 worldMatrix;
    OBJ_ATTRIBUTES material;
};
// This is how you declare and access a Vulkan storage buffer in HLSL
StructuredBuffer<SHADER_MODEL_DATA> SceneData : register(b1);

cbuffer SHADER_SCENE_DATA : register(b0)
{
	float4 sunDirection, sunColor, sunAmbient, camPos;
	float4x4 viewMatrix, projectionMatrix;
};

// an ultra simple hlsl pixel shader
float4 main(float4 pos : SV_POSITION, float3 nrm : NORMAL,
            float3 posW : WORLD, float3 uvw : TEXCOORD, uint index : INDEX) : SV_TARGET
{
    float4 diffuse = float4(SceneData[index].material.Kd, 1);
    float4 specular = float4(SceneData[index].material.Ks, 1);
    float4 ambient = float4(SceneData[index].material.Ka, 1);
    float4 emissive = float4(SceneData[index].material.Ke, 1);
    float lightRatio = saturate(dot(-sunDirection, normalize(nrm)));

    float3 viewDir = normalize(camPos.xyz - posW);
    float3 bounce = reflect(sunDirection, normalize(nrm));
    float intense = max(pow(saturate(dot(viewDir, bounce)), SceneData[index].material.Ns), 0);
    float4 reflected = sunColor * specular * intense;

    float4 direct = lightRatio * sunColor;
    float4 indirect = sunAmbient * ambient;

    return saturate(direct + indirect) * diffuse + reflected + emissive;
}