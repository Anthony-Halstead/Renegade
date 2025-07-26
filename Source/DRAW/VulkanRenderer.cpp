#include "DrawComponents.h"
#include "../CCL.h"
// component dependencies
#include "./Utility/FileIntoString.h"
#include "../TextureUtils.h"
#include "../UI/UIComponents.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"
#include "shaderc/shaderc.h" // needed for compiling shaders at runtime
#ifdef _WIN32 // must use MT platform DLL libraries on windows
#pragma comment(lib, "shaderc_combined.lib") 
#endif

namespace DRAW
{
	////*** HELPER METHODS ***//

	std::string ExtractFilePath(const char* map_Kd_raw)
	{
		if (!map_Kd_raw) return "";
		std::string input(map_Kd_raw);

		size_t lastSlash = input.find_last_of("/\\");
		if (lastSlash != std::string::npos) {
			input = input.substr(lastSlash + 1);
		}

		size_t lastSpace = input.find_last_of(' ');
		if (lastSpace != std::string::npos) {
			input = input.substr(lastSpace + 1);
		}

		input.erase(0, input.find_first_not_of(' '));
		input.erase(input.find_last_not_of(' ') + 1);

		if (
			input.length() < 5 ||
			(input.find(".png") == std::string::npos &&
				input.find(".jpeg") == std::string::npos &&
				input.find(".jpg") == std::string::npos)
			)
			return "";

		return input;
	}

	void Construct_Textures(entt::registry& registry, entt::entity entity)
	{
		using namespace DRAW;
		CPULevel& cpuLevel = registry.get<CPULevel>(entity);
		Level_Data& level = cpuLevel.levelData;
		VulkanRenderer& vkRenderer = registry.get<VulkanRenderer>(entity);

		const char* TEXTURE_DIR = "../Models/Textures/";

		std::map<std::string, int> albedoFileToIndex, normalFileToIndex, emissiveFileToIndex,
			alphaFileToIndex, specularFileToIndex;

		auto loadTextureMap = [&](const char* texPath, std::vector<TextureData>& texVec, std::map<std::string, int>& fileToIdx)
			{
				std::string fname = ExtractFilePath(texPath);
				if (!fname.empty() && fileToIdx.count(fname) == 0) {
					std::string fullPath = std::string(TEXTURE_DIR) + fname;
					std::ifstream f(fullPath);
					if (!f.good()) {
#if NDEBUG
						std::cout << "Texture file [" << fullPath << "] does NOT exist!\n";
#endif
						fileToIdx[fname] = -1;
						return;
					}
#if NDEBUG
					std::cout << "Texture file [" << fullPath << "] FOUND!\n";
#endif
					TextureData texData{};
					UploadTextureToGPU(vkRenderer.vlkSurface, fullPath.c_str(), texData.memory, texData.image, texData.imageView);
					texVec.push_back(texData);
					fileToIdx[fname] = static_cast<int>(texVec.size()) - 1;
				}
			};

		for (size_t i = 0; i < level.levelMaterials.size(); ++i) {
			const H2B::MATERIAL& mat = level.levelMaterials[i];
			loadTextureMap(mat.map_Kd, vkRenderer.textures, albedoFileToIndex);
			loadTextureMap(mat.bump, vkRenderer.texturesNormal, normalFileToIndex);
			loadTextureMap(mat.map_Ke, vkRenderer.texturesEmissive, emissiveFileToIndex);
			loadTextureMap(mat.map_d, vkRenderer.texturesAlpha, alphaFileToIndex);
			loadTextureMap(mat.map_Ks, vkRenderer.texturesSpecular, specularFileToIndex);
		}

		auto ensureFallback = [&](std::vector<TextureData>& texVec, const char* fallbackFile) {
			if (texVec.empty()) {
				TextureData fallback{};
				UploadTextureToGPU(vkRenderer.vlkSurface, fallbackFile, fallback.memory, fallback.image, fallback.imageView);
				texVec.push_back(fallback);
			}
			};

		ensureFallback(vkRenderer.textures, "../Models/Textures/fallback_albedo.png");
		ensureFallback(vkRenderer.texturesNormal, "../Models/Textures/fallback_normal.png");
		ensureFallback(vkRenderer.texturesEmissive, "../Models/Textures/fallback_emissive.png");
		ensureFallback(vkRenderer.texturesAlpha, "../Models/Textures/fallback_alpha.png");
		ensureFallback(vkRenderer.texturesSpecular, "../Models/Textures/fallback_specular.png");

		level.levelTextures.resize(level.levelMaterials.size());
		for (size_t i = 0; i < level.levelMaterials.size(); ++i) {
			const H2B::MATERIAL& mat = level.levelMaterials[i];
			auto& tex = level.levelTextures[i];

			tex.albedoIndex = -1;
			if (mat.map_Kd && mat.map_Kd[0]) {
				std::string fname = ExtractFilePath(mat.map_Kd);
				auto it = albedoFileToIndex.find(fname);
				if (it != albedoFileToIndex.end())
					tex.albedoIndex = it->second;
			}

			tex.normalIndex = -1;
			if (mat.bump && mat.bump[0]) {
				std::string fname = ExtractFilePath(mat.bump);
				auto it = normalFileToIndex.find(fname);
				if (it != normalFileToIndex.end())
					tex.normalIndex = it->second;
			}

			tex.emissiveIndex = -1;
			if (mat.map_Ke && mat.map_Ke[0]) {
				std::string fname = ExtractFilePath(mat.map_Ke);
				auto it = emissiveFileToIndex.find(fname);
				if (it != emissiveFileToIndex.end())
					tex.emissiveIndex = it->second;
			}

			tex.alphaIndex = -1;
			if (mat.map_d && mat.map_d[0]) {
				std::string fname = ExtractFilePath(mat.map_d);
				auto it = alphaFileToIndex.find(fname);
				if (it != alphaFileToIndex.end())
					tex.alphaIndex = it->second;
			}

			tex.specularIndex = -1;
			if (mat.map_Ks && mat.map_Ks[0]) {
				std::string fname = ExtractFilePath(mat.map_Ks);
				auto it = specularFileToIndex.find(fname);
				if (it != specularFileToIndex.end())
					tex.specularIndex = it->second;
			}
		}

		if (vkRenderer.textureSampler == VK_NULL_HANDLE) {
			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.maxAnisotropy = 1.0f;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			if (vkCreateSampler(vkRenderer.device, &samplerInfo, nullptr, &vkRenderer.textureSampler) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create Vulkan texture sampler!");
			}
		}
	}


	VkViewport CreateViewportFromWindowDimensions(unsigned int windowWidth, unsigned int windowHeight)
	{
		VkViewport retval = {};
		retval.x = 0;
		retval.y = 0;
		retval.width = static_cast<float>(windowWidth);
		retval.height = static_cast<float>(windowHeight);
		retval.minDepth = 0;
		retval.maxDepth = 1;
		return retval;
	}

	VkRect2D CreateScissorFromWindowDimensions(unsigned int windowWidth, unsigned int windowHeight)
	{
		VkRect2D retval = {};
		retval.offset.x = 0;
		retval.offset.y = 0;
		retval.extent.width = windowWidth;
		retval.extent.height = windowHeight;
		return retval;
	}

	void InitializeDescriptors(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);

		unsigned int frameCount;
		vulkanRenderer.vlkSurface.GetSwapchainImageCount(frameCount);
		vulkanRenderer.descriptorSets.resize(frameCount);

#pragma region Descriptor Layout (Uniform/Storage)
		VkDescriptorSetLayoutBinding layoutBinding[2] = {};
		layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding[0].descriptorCount = 1;
		layoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBinding[0].binding = 0;

		layoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		layoutBinding[1].descriptorCount = 1;
		layoutBinding[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		layoutBinding[1].binding = 1;

		VkDescriptorSetLayoutCreateInfo setCreateInfo = {};
		setCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setCreateInfo.bindingCount = 2;
		setCreateInfo.pBindings = layoutBinding;
		setCreateInfo.flags = 0;
		setCreateInfo.pNext = nullptr;
		vkCreateDescriptorSetLayout(vulkanRenderer.device, &setCreateInfo, nullptr, &vulkanRenderer.descriptorLayout);
#pragma endregion

#pragma region Texture Descriptor Layout (6 Bindings)
		uint32_t maxAlbedo = static_cast<uint32_t>(vulkanRenderer.textures.size());
		uint32_t maxNormal = static_cast<uint32_t>(vulkanRenderer.texturesNormal.size());
		uint32_t maxEmissive = static_cast<uint32_t>(vulkanRenderer.texturesEmissive.size());
		uint32_t maxAlpha = static_cast<uint32_t>(vulkanRenderer.texturesAlpha.size());
		uint32_t maxSpecular = static_cast<uint32_t>(vulkanRenderer.texturesSpecular.size());

		VkDescriptorSetLayoutBinding texBindings[5] = {};

		texBindings[0].binding = 0;
		texBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		texBindings[0].descriptorCount = maxAlbedo;
		texBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		texBindings[1].binding = 1;
		texBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		texBindings[1].descriptorCount = maxNormal;
		texBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		texBindings[2].binding = 2;
		texBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		texBindings[2].descriptorCount = maxEmissive;
		texBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		texBindings[3].binding = 3;
		texBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		texBindings[3].descriptorCount = maxAlpha;
		texBindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		texBindings[4].binding = 4;
		texBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		texBindings[4].descriptorCount = maxSpecular;
		texBindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo texLayoutInfo = {};
		texLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		texLayoutInfo.bindingCount = 5;
		texLayoutInfo.pBindings = texBindings;

		vkCreateDescriptorSetLayout(
			vulkanRenderer.device, &texLayoutInfo, nullptr, &vulkanRenderer.textureSetLayout
		);
#pragma endregion

#pragma region Descriptor Pool
		VkDescriptorPoolSize poolSizes[7] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,   frameCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,   frameCount },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxAlbedo },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxNormal },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxEmissive },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxAlpha },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSpecular }
		};

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 7;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = frameCount * 2;
		poolInfo.flags = 0;
		poolInfo.pNext = nullptr;
		vkCreateDescriptorPool(vulkanRenderer.device, &poolInfo, nullptr, &vulkanRenderer.descriptorPool);
#pragma endregion

		vulkanRenderer.textureSetLayouts.resize(frameCount);
		for (uint32_t i = 0; i < frameCount; ++i)
			vulkanRenderer.textureSetLayouts[i] = vulkanRenderer.textureSetLayout;

		VkDescriptorSetAllocateInfo texAllocInfo = {};
		texAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		texAllocInfo.descriptorPool = vulkanRenderer.descriptorPool;
		texAllocInfo.descriptorSetCount = frameCount;
		texAllocInfo.pSetLayouts = vulkanRenderer.textureSetLayouts.data();
		texAllocInfo.pNext = nullptr;

		vulkanRenderer.textureDescriptorSets.resize(frameCount);
		if (vkAllocateDescriptorSets(vulkanRenderer.device, &texAllocInfo, vulkanRenderer.textureDescriptorSets.data()) != VK_SUCCESS)
			throw std::runtime_error("Failed to allocate texture descriptor sets!");

		for (uint32_t i = 0; i < frameCount; ++i) {
			std::vector<VkWriteDescriptorSet> writes(5);

			auto createImageInfos = [vulkanRenderer](const std::vector<TextureData>& texVec) {
				std::vector<VkDescriptorImageInfo> infos;
				for (const auto& tex : texVec) {
					VkDescriptorImageInfo info = {};
					info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					info.imageView = tex.imageView;
					info.sampler = vulkanRenderer.textureSampler;
					infos.push_back(info);
				}
				return infos;
				};

			auto albedoInfos = createImageInfos(vulkanRenderer.textures);
			auto normalInfos = createImageInfos(vulkanRenderer.texturesNormal);
			auto emissiveInfos = createImageInfos(vulkanRenderer.texturesEmissive);
			auto alphaInfos = createImageInfos(vulkanRenderer.texturesAlpha);
			auto specularInfos = createImageInfos(vulkanRenderer.texturesSpecular);


			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet = vulkanRenderer.textureDescriptorSets[i];
			writes[0].dstBinding = 0;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[0].descriptorCount = albedoInfos.size();
			writes[0].pImageInfo = albedoInfos.data();

			writes[1] = writes[0]; writes[1].dstBinding = 1; writes[1].descriptorCount = normalInfos.size(); writes[1].pImageInfo = normalInfos.data();
			writes[2] = writes[0]; writes[2].dstBinding = 2; writes[2].descriptorCount = emissiveInfos.size(); writes[2].pImageInfo = emissiveInfos.data();
			writes[3] = writes[0]; writes[3].dstBinding = 3; writes[3].descriptorCount = alphaInfos.size(); writes[3].pImageInfo = alphaInfos.data();
			writes[4] = writes[0]; writes[4].dstBinding = 4; writes[4].descriptorCount = specularInfos.size(); writes[4].pImageInfo = specularInfos.data();

			vkUpdateDescriptorSets(vulkanRenderer.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		}

#pragma region Allocate Descriptor Sets (Uniform/Storage)
		VkDescriptorSetAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.descriptorPool = vulkanRenderer.descriptorPool;
		allocateInfo.pSetLayouts = &vulkanRenderer.descriptorLayout;
		allocateInfo.pNext = nullptr;
		for (int i = 0; i < frameCount; i++)
		{
			vkAllocateDescriptorSets(vulkanRenderer.device, &allocateInfo, &vulkanRenderer.descriptorSets[i]);
		}
#pragma endregion

		auto& storageBuffer = registry.emplace<VulkanGPUInstanceBuffer>(entity, VulkanGPUInstanceBuffer{ 16 });
		auto& uniformBuffer = registry.emplace<VulkanUniformBuffer>(entity);

		for (int i = 0; i < frameCount; i++)
		{
			VkDescriptorBufferInfo uniformBufferInfo = { uniformBuffer.buffer[i], 0, VK_WHOLE_SIZE };
			VkWriteDescriptorSet uniformWrite = {};
			uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			uniformWrite.dstSet = vulkanRenderer.descriptorSets[i];
			uniformWrite.dstBinding = 0;
			uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uniformWrite.descriptorCount = 1;
			uniformWrite.pBufferInfo = &uniformBufferInfo;

			VkDescriptorBufferInfo storageBufferInfo = { storageBuffer.buffer[i], 0, VK_WHOLE_SIZE };
			VkWriteDescriptorSet storageWrite = {};
			storageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			storageWrite.dstSet = vulkanRenderer.descriptorSets[i];
			storageWrite.dstBinding = 1;
			storageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			storageWrite.descriptorCount = 1;
			storageWrite.pBufferInfo = &storageBufferInfo;

			VkWriteDescriptorSet descriptorWrites[] = { uniformWrite, storageWrite };
			vkUpdateDescriptorSets(vulkanRenderer.device, 2, descriptorWrites, 0, nullptr);
		}
	}

	void InitializeGraphicsPipeline(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		GW::SYSTEM::GWindow win = registry.get<GW::SYSTEM::GWindow>(entity);

		// Create Pipeline & Layout (Thanks Tiny!)
		VkPipelineShaderStageCreateInfo stage_create_info[2] = {};
		// Create Stage Info for Vertex Shader
		stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_create_info[0].module = vulkanRenderer.vertexShader;
		stage_create_info[0].pName = "main";

		// Create Stage Info for Fragment Shader
		stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_create_info[1].module = vulkanRenderer.fragmentShader;
		stage_create_info[1].pName = "main";

		VkPipelineInputAssemblyStateCreateInfo assembly_create_info = {};
		assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		assembly_create_info.primitiveRestartEnable = false;

		VkVertexInputBindingDescription vertex_binding_description = {};
		vertex_binding_description.binding = 0;
		vertex_binding_description.stride = sizeof(H2B::VERTEX);
		vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertex_attribute_description[3];
		vertex_attribute_description[0].binding = 0;
		vertex_attribute_description[0].location = 0;
		vertex_attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[0].offset = offsetof(H2B::VERTEX, pos);

		vertex_attribute_description[1].binding = 0;
		vertex_attribute_description[1].location = 1;
		vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[1].offset = offsetof(H2B::VERTEX, uvw);

		vertex_attribute_description[2].binding = 0;
		vertex_attribute_description[2].location = 2;
		vertex_attribute_description[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description[2].offset = offsetof(H2B::VERTEX, nrm);

		VkPipelineVertexInputStateCreateInfo input_vertex_info = {};
		input_vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		input_vertex_info.vertexBindingDescriptionCount = 1;
		input_vertex_info.pVertexBindingDescriptions = &vertex_binding_description;
		input_vertex_info.vertexAttributeDescriptionCount = 3;
		input_vertex_info.pVertexAttributeDescriptions = vertex_attribute_description;

		unsigned int windowWidth, windowHeight;
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);


		VkViewport viewport = CreateViewportFromWindowDimensions(windowWidth, windowHeight);

		VkRect2D scissor = CreateScissorFromWindowDimensions(windowWidth, windowHeight);

		VkPipelineViewportStateCreateInfo viewport_create_info = {};
		viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_create_info.viewportCount = 1;
		viewport_create_info.pViewports = &viewport;
		viewport_create_info.scissorCount = 1;
		viewport_create_info.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterization_create_info = {};
		rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_create_info.lineWidth = 1.0f;
		rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterization_create_info.depthClampEnable = VK_FALSE;
		rasterization_create_info.depthBiasEnable = VK_FALSE;
		rasterization_create_info.depthBiasClamp = 0.0f;
		rasterization_create_info.depthBiasConstantFactor = 0.0f;
		rasterization_create_info.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
		multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_create_info.sampleShadingEnable = VK_FALSE;
		multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_create_info.minSampleShading = 1.0f;
		multisample_create_info.pSampleMask = VK_NULL_HANDLE;
		multisample_create_info.alphaToCoverageEnable = VK_FALSE;
		multisample_create_info.alphaToOneEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = {};
		depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_create_info.depthTestEnable = VK_TRUE;
		depth_stencil_create_info.depthWriteEnable = VK_TRUE;
		depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_create_info.minDepthBounds = 0.0f;
		depth_stencil_create_info.maxDepthBounds = 1.0f;
		depth_stencil_create_info.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
		color_blend_attachment_state.colorWriteMask = 0xF;
		color_blend_attachment_state.blendEnable = VK_FALSE;
		color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
		color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
		color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
		color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo color_blend_create_info = {};
		color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_create_info.logicOpEnable = VK_FALSE;
		color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
		color_blend_create_info.attachmentCount = 1;
		color_blend_create_info.pAttachments = &color_blend_attachment_state;
		color_blend_create_info.blendConstants[0] = 0.0f;
		color_blend_create_info.blendConstants[1] = 0.0f;
		color_blend_create_info.blendConstants[2] = 0.0f;
		color_blend_create_info.blendConstants[3] = 0.0f;

		// Dynamic State 
		VkDynamicState dynamic_states[2] =
		{
			// By setting these we do not need to re-create the pipeline on Resize
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_create_info = {};
		dynamic_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_create_info.dynamicStateCount = 2;
		dynamic_create_info.pDynamicStates = dynamic_states;
		Construct_Textures(registry, entity);
		InitializeDescriptors(registry, entity);
		VkDescriptorSetLayout setLayouts[2] = {
	vulkanRenderer.descriptorLayout,    // set=0: buffer set
	vulkanRenderer.textureSetLayout     // set=1: texture set
		};

		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 2;
		pipeline_layout_create_info.pSetLayouts = setLayouts;
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.pPushConstantRanges = nullptr;

		vkCreatePipelineLayout(
			vulkanRenderer.device,
			&pipeline_layout_create_info,
			nullptr,
			&vulkanRenderer.pipelineLayout
		);

		// Pipeline State... (FINALLY) 
		VkGraphicsPipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = stage_create_info;
		pipeline_create_info.pInputAssemblyState = &assembly_create_info;
		pipeline_create_info.pVertexInputState = &input_vertex_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.pDynamicState = &dynamic_create_info;
		pipeline_create_info.layout = vulkanRenderer.pipelineLayout;
		pipeline_create_info.renderPass = vulkanRenderer.renderPass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

		vkCreateGraphicsPipelines(vulkanRenderer.device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &vulkanRenderer.pipeline);
	}

	//*** SYSTEMS ***//

	// run this code when a VulkanRenderer component is connected
	void Construct_VulkanRenderer(entt::registry& registry, entt::entity entity)
	{
		if (!registry.all_of<GW::SYSTEM::GWindow>(entity))
		{
			std::cout << "Window not added to the registry yet!" << std::endl;
			abort();
			return;
		}

		if (!registry.all_of<VulkanRendererInitialization>(entity))
		{
			std::cout << "Initialization Data not added to the registry yet!" << std::endl;
			abort();
			return;
		}

		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);
		auto& initializationData = registry.get<VulkanRendererInitialization>(entity);

		GW::SYSTEM::GWindow win = registry.get<GW::SYSTEM::GWindow>(entity);
#ifndef NDEBUG
		const char* debugLayers[] = {
			"VK_LAYER_KHRONOS_validation", // standard validation layer
		};
		if (-vulkanRenderer.vlkSurface.Create(win, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT | GW::GRAPHICS::GGraphicsInitOptions::BINDLESS_SUPPORT,
			sizeof(debugLayers) / sizeof(debugLayers[0]),
			debugLayers, 0, nullptr, 0, nullptr, false))
#else
		if (-vulkanRenderer.vlkSurface.Create(win, GW::GRAPHICS::DEPTH_BUFFER_SUPPORT))
#endif
		{
			std::cout << "Failed to create Vulkan Surface!" << std::endl;
			abort();
			return;
		}

		vulkanRenderer.clrAndDepth[0].color = VkClearColorValue{ {0.0f, 0.0f, 0.0f, 1.0f} };
		vulkanRenderer.clrAndDepth[1].depthStencil = initializationData.depthStencil;

		// Create Projection matrix
		float aspectRatio;
		vulkanRenderer.vlkSurface.GetAspectRatio(aspectRatio);
		GW::MATH::GMatrix::ProjectionVulkanLHF(G2D_DEGREE_TO_RADIAN_F(initializationData.fovDegrees), aspectRatio, initializationData.nearPlane, initializationData.farPlane, vulkanRenderer.projMatrix);


		vulkanRenderer.vlkSurface.GetDevice((void**)&vulkanRenderer.device);
		vulkanRenderer.vlkSurface.GetPhysicalDevice((void**)&vulkanRenderer.physicalDevice);
		vulkanRenderer.vlkSurface.GetRenderPass((void**)&vulkanRenderer.renderPass);

		// Intialize runtime shader compiler HLSL -> SPIRV
		shaderc_compiler_t compiler = shaderc_compiler_initialize();
		shaderc_compile_options_t options = shaderc_compile_options_initialize();
		shaderc_compile_options_set_source_language(options, shaderc_source_language_hlsl);
		shaderc_compile_options_set_invert_y(options, false);
#ifndef NDEBUG
		shaderc_compile_options_set_generate_debug_info(options);
#endif

		// Vertex Shader
		std::string vertexShaderSource = ReadFileIntoString(initializationData.vertexShaderName.c_str());

		shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
			compiler, vertexShaderSource.c_str(), vertexShaderSource.length(),
			shaderc_vertex_shader, "main.vert", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			std::cout << "Vertex Shader Errors : \n" << shaderc_result_get_error_message(result) << std::endl;
			abort();
			return;
		}

		GvkHelper::create_shader_module(vulkanRenderer.device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vulkanRenderer.vertexShader);

		shaderc_result_release(result); // done

		// Fragment Shader
		std::string fragmentShaderSource = ReadFileIntoString(initializationData.fragmentShaderName.c_str());

		result = shaderc_compile_into_spv( // compile
			compiler, fragmentShaderSource.c_str(), fragmentShaderSource.length(),
			shaderc_fragment_shader, "main.frag", "main", options);

		if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
		{
			std::cout << "Fragment Shader Errors : \n" << shaderc_result_get_error_message(result) << std::endl;
			abort();
			return;
		}

		GvkHelper::create_shader_module(vulkanRenderer.device, shaderc_result_get_length(result), // load into Vulkan
			(char*)shaderc_result_get_bytes(result), &vulkanRenderer.fragmentShader);

		shaderc_result_release(result); // done

		// Free runtime shader compiler resources
		shaderc_compile_options_release(options);
		shaderc_compiler_release(compiler);

		InitializeGraphicsPipeline(registry, entity);

		// Remove the initializtion data as we no longer need it
		registry.remove<VulkanRendererInitialization>(entity);

	}

	// run this code when a VulkanRenderer component is updated
	void Update_VulkanRenderer(entt::registry& registry, entt::entity entity)
	{
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);

		if (-vulkanRenderer.vlkSurface.StartFrame(2, vulkanRenderer.clrAndDepth))
		{
			std::cout << "Failed to start frame!" << std::endl;
			return;
		}

		auto win = registry.get<GW::SYSTEM::GWindow>(entity);
		unsigned int frame;
		vulkanRenderer.vlkSurface.GetSwapchainCurrentImage(frame);

		VkCommandBuffer commandBuffer;
		unsigned int currentBuffer;
		vulkanRenderer.vlkSurface.GetSwapchainCurrentImage(currentBuffer);
		vulkanRenderer.vlkSurface.GetCommandBuffer(currentBuffer, (void**)&commandBuffer);

		unsigned int windowWidth, windowHeight;
		win.GetClientWidth(windowWidth);
		win.GetClientHeight(windowHeight);
		VkViewport viewport = CreateViewportFromWindowDimensions(windowWidth, windowHeight);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = CreateScissorFromWindowDimensions(windowWidth, windowHeight);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanRenderer.pipeline);

		// Update uniform and storage buffers
		registry.patch<VulkanUniformBuffer>(entity);

		entt::basic_group entityGroup = registry.group<entt::entity>(entt::get<GPUInstance, GeometryData>, entt::exclude<DoNotRender>);
		entityGroup.sort<GeometryData>([](const GeometryData& a, const GeometryData& b) { return a < b; });

		// Check for presence of the buffers first as they take a few frames before they are created
		if (registry.all_of< VulkanVertexBuffer, VulkanIndexBuffer>(entity))
		{
			auto& vertexBuffer = registry.get<VulkanVertexBuffer>(entity);
			auto& indexBuffer = registry.get<VulkanIndexBuffer>(entity);

			if (vertexBuffer.buffer != VK_NULL_HANDLE && indexBuffer.buffer != VK_NULL_HANDLE)
			{
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
				vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);
			}

			std::vector<GPUInstance> instanceData;
			std::map<GeometryData, int> geometryData;

			for (entt::entity ent : entityGroup)
			{
				instanceData.push_back(registry.get<GPUInstance>(ent));

				GeometryData geometry = registry.get<GeometryData>(ent);
				std::map<GeometryData, int>::iterator it = geometryData.find(geometry);

				if (it != geometryData.end()) ++(it->second);
				else geometryData.insert({ geometry, 1 });
			}

			registry.emplace<std::vector<GPUInstance>>(entity, instanceData);
			registry.patch<VulkanGPUInstanceBuffer>(entity);

			VkDescriptorSet sets[2] = {
	vulkanRenderer.descriptorSets[frame],
	vulkanRenderer.textureDescriptorSets[frame]
			};
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				vulkanRenderer.pipelineLayout,
				0,
				2,
				sets,
				0, nullptr
			);
			unsigned instance = 0;

			for (std::map<GeometryData, int>::iterator it = geometryData.begin(); it != geometryData.end(); ++it)
			{
				vkCmdDrawIndexed(commandBuffer, it->first.indexCount, it->second, it->first.indexStart, it->first.vertexStart, instance);
				instance += it->second;
			}
		}

		registry.patch<UI::UIManager>(registry.view<UI::UIManager>().front());

		vulkanRenderer.vlkSurface.EndFrame(true);
	}

	// run this code when a VulkanRenderer component is updated
	void Destroy_VulkanRenderer(entt::registry& registry, entt::entity entity)
	{
		registry.remove<UI::UIManager>(registry.view<UI::UIManager>().front());
		
		auto& vulkanRenderer = registry.get<VulkanRenderer>(entity);

		vkDeviceWaitIdle(vulkanRenderer.device);

		auto destroyTextureVec = [&](std::vector<TextureData>& vec) {
			if (vec.empty())return;
			for (auto& t : vec) {

				if (t.buffer != VK_NULL_HANDLE)
					vkDestroyBuffer(vulkanRenderer.device, t.buffer, nullptr);
				if (t.imageView != VK_NULL_HANDLE)
					vkDestroyImageView(vulkanRenderer.device, t.imageView, nullptr);
				if (t.image != VK_NULL_HANDLE)
					vkDestroyImage(vulkanRenderer.device, t.image, nullptr);
				if (t.memory != VK_NULL_HANDLE)
					vkFreeMemory(vulkanRenderer.device, t.memory, nullptr);

			}
			vec.clear();
			};

		destroyTextureVec(vulkanRenderer.textures);
		destroyTextureVec(vulkanRenderer.texturesNormal);
		destroyTextureVec(vulkanRenderer.texturesEmissive);
		destroyTextureVec(vulkanRenderer.texturesAlpha);
		destroyTextureVec(vulkanRenderer.texturesSpecular);
		registry.remove<VulkanIndexBuffer>(entity);
		registry.remove<VulkanVertexBuffer>(entity);
		registry.remove<VulkanGPUInstanceBuffer>(entity);
		registry.remove<VulkanUniformBuffer>(entity);
		if (vulkanRenderer.textureSetLayout)
			vkDestroyDescriptorSetLayout(vulkanRenderer.device, vulkanRenderer.textureSetLayout, nullptr);

		if (vulkanRenderer.textureSampler)
			vkDestroySampler(vulkanRenderer.device, vulkanRenderer.textureSampler, nullptr);


		vkDestroyDescriptorSetLayout(vulkanRenderer.device, vulkanRenderer.descriptorLayout, nullptr);
		vkDestroyDescriptorPool(vulkanRenderer.device, vulkanRenderer.descriptorPool, nullptr);

		vkDestroyShaderModule(vulkanRenderer.device, vulkanRenderer.vertexShader, nullptr);
		vkDestroyShaderModule(vulkanRenderer.device, vulkanRenderer.fragmentShader, nullptr);
		vkDestroyPipelineLayout(vulkanRenderer.device, vulkanRenderer.pipelineLayout, nullptr);
		vkDestroyPipeline(vulkanRenderer.device, vulkanRenderer.pipeline, nullptr);
	}

	// Use this MACRO to connect the EnTT Component Logic
	CONNECT_COMPONENT_LOGIC() {
		// register the Window component's logic
		registry.on_construct<VulkanRenderer>().connect<Construct_VulkanRenderer>();
		registry.on_update<VulkanRenderer>().connect<Update_VulkanRenderer>();
		registry.on_destroy<VulkanRenderer>().connect<Destroy_VulkanRenderer>();
	}

} // namespace DRAW