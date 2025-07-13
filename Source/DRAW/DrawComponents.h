#ifndef DRAW_COMPONENTS_H
#define DRAW_COMPONENTS_H


#include "./Utility/load_data_oriented.h"

namespace DRAW
{
	//*** TAGS ***//

	//*** COMPONENTS ***//
	struct VulkanRendererInitialization
	{
		std::string vertexShaderName;
		std::string fragmentShaderName;
		VkClearColorValue clearColor;
		VkClearDepthStencilValue depthStencil;
		float fovDegrees;
		float nearPlane;
		float farPlane;
	};

	struct TextureData
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
		VkImage image;
		VkImageView imageView;
	};

	struct VulkanRenderer
	{
		GW::GRAPHICS::GVulkanSurface vlkSurface;
		VkDevice device = nullptr;
		VkPhysicalDevice physicalDevice = nullptr;
		VkRenderPass renderPass;
		VkShaderModule vertexShader = nullptr;
		VkShaderModule fragmentShader = nullptr;
		VkPipeline pipeline = nullptr;
		VkPipelineLayout pipelineLayout = nullptr;
		GW::MATH::GMATRIXF projMatrix;
		VkDescriptorSetLayout descriptorLayout = nullptr;
		VkDescriptorPool descriptorPool = nullptr;
		std::vector<VkDescriptorSet> descriptorSets;

		VkDescriptorSetLayout textureSetLayout;
		VkDescriptorSet textureDescriptorSet;
		std::vector<VkDescriptorSet> textureDescriptorSets;
		std::vector<VkDescriptorSetLayout>textureSetLayouts;
		VkSampler textureSampler = VK_NULL_HANDLE;
		VkClearValue clrAndDepth[2];

		std::vector<TextureData> textures;
		std::vector<TextureData> texturesNormal;
		std::vector<TextureData> texturesEmissive;
		std::vector<TextureData> texturesAlpha;
		std::vector<TextureData> texturesSpecular;
	};

	struct VulkanVertexBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	struct VulkanIndexBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	struct GeometryData
	{
		unsigned int indexStart, indexCount, vertexStart;
		inline bool operator < (const GeometryData a) const {
			return indexStart < a.indexStart;
		}
	};

	struct GPUInstance
	{
		GW::MATH::GMATRIXF	transform;
		H2B::ATTRIBUTES		matData;
		uint32_t albedoIndex;
		uint32_t normalIndex;
		uint32_t emissiveIndex;
		uint32_t alphaIndex;
		uint32_t specularIndex;
	};

	struct VulkanGPUInstanceBuffer
	{
		unsigned long long element_count = 1;
		unsigned long long min_capacity = 16;
		unsigned idleFrames = 0;
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;
	};

	struct SceneData
	{
		float time;
		float _pad[3];
		GW::MATH::GVECTORF sunDirection, sunColor, sunAmbient, camPos;
		GW::MATH::GMATRIXF viewMatrix, projectionMatrix;
	};

	struct VulkanUniformBuffer
	{
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;
	};


	struct Camera
	{
		GW::MATH::GMATRIXF camMatrix;
	};


	struct CPULevel {
		std::string levelFile;
		std::string modelPath;
		Level_Data levelData;
	};

	struct GPULevel {};

	struct DoNotRender {};

	struct MeshCollection {
		std::vector<entt::entity> entities;
		GW::MATH::GOBBF obb;
	};
	struct OBB {
		GW::MATH::GOBBF obb;
	};
	struct ModelManager {
		std::map<std::string, MeshCollection> models;
	};

} // namespace DRAW
#endif // !DRAW_COMPONENTS_H
