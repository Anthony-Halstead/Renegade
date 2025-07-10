// Gateware Research file. Will be used to test adding an asynchronous overlay to GVulkanSurface
#pragma once

// Author: Lari Norri 12/31/2024
// This is protype class for an overlay that can be updated asynchronously
// and rendered using Vulkan. The overlay is intended to be used for HUDs,
// menus, and other 2D elements that need to be rendered on top of a 3D scene.
// The overlay is created with a specific resolution and can be updated with
// pixel data. The overlay can be presented in a variety of ways including
// scaling, alignment, and interpolation. The overlay is updated asynchronously
// to avoid blocking the main rendering thread. The overlay is rendered using
// Vulkan to maximize performance and compatibility with modern hardware.

// This software is in beta stage and is provided as-is with no warranty.
// The software is provided under the MIT license and will be incorporated directly
// into the Gateware API in the future. The software is provided as a single header	
// file and two HLSL shaders. The software requires the Vulkan SDK to be installed	
// on the system and the shaderc library to be linked. The software is intended to be
// used with the Gateware API and requires the Gateware API included before this header.

// Many aspects of this class are pulled from Gateware's GRasterSurface.
// Thanks to Artemis K. Frost for the alignment techniques used in this class.
class Overlay
{
	GW::CORE::GThreadShared lock; // safe async access to CPU staging buffer
	GW::SYSTEM::GWindow windowHandle;
	GW::CORE::GEventResponder shutdown;
	GW::GRAPHICS::GVulkanSurface surfaceHandle;
	unsigned int presentStyle = 0;
	std::atomic_int64_t overlayUpdateCount;

	// CPU Stagging Buffer, only one is needed (ground truth)
	VkBuffer stagingBuffer = nullptr;
	VkDeviceMemory stagingBufferMemory = nullptr;
	// Internal resolution of the overlay
	unsigned int width = 0;
	unsigned int height = 0;

	// struct to represent each swap chain image's overlay data
	struct OverlayImage
	{
		VkDeviceMemory memory = nullptr;
		VkImage image = nullptr;
		VkImageView imageView = nullptr;
		VkDescriptorSet descriptorSet = nullptr;
		std::int64_t lastUpdate = 0;
	};
	std::vector<OverlayImage> overlayImages;

	// push constants for the overlay
	struct OverlayConstants
	{
		float offset[2] = { 0, 0 };
		float scale[2] = { 1, 1 };
	}overlayConstants;

	// create Vulkan objects used to manage rendering the overlay
	VkDevice device = nullptr;
	VkRenderPass renderPass = nullptr;
	VkPhysicalDevice physicalDevice = nullptr;
	VkCommandPool commandPool = nullptr;
	VkQueue graphicsQueue = nullptr;
	// vertex and fragment shaders for the overlay
	VkShaderModule vertexShader = nullptr;
	VkShaderModule fragmentShader = nullptr;
	// create a descriptor set layout for the overlay
	VkDescriptorSetLayout descriptorSetLayout = nullptr;
	// create a descriptor pool for the overlay
	VkDescriptorPool descriptorPool = nullptr;
	// create a pipeline layout for the overlay
	VkPipelineLayout pipelineLayout = nullptr;
	// create a pipeline for the overlay
	VkPipeline pipeline = nullptr;
	// create a sampler for the overlay
	VkSampler sampler = nullptr;

	// TEMPORARY COMPILER OPERATIONS
	void CompileShaders();

	shaderc_compile_options_t CreateCompileOptions();

	void CompileVertexShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options);

	void CompileFragmentShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options);

	// Cleanup
	void Cleanup();

	// GRasterUpdateFlags bit positions for left/right shifting
	enum UpdateFlagBitPosition
	{
		BIT_ALIGN_X_LEFT = 0,
		BIT_ALIGN_X_CENTER,
		BIT_ALIGN_X_RIGHT,
		BIT_ALIGN_Y_TOP,
		BIT_ALIGN_Y_CENTER,
		BIT_ALIGN_Y_BOTTOM,
		BIT_UPSCALE_2X,
		BIT_UPSCALE_3X,
		BIT_UPSCALE_4X,
		BIT_UPSCALE_8X,
		BIT_UPSCALE_16X,
		BIT_STRETCH_TO_FIT,
		BIT_INTERPOLATE_NEAREST,
		BIT_INTERPOLATE_BILINEAR,
	};
	// separate flags into sections and test each section
	const unsigned int bitmaskAlignX =
		GW::GRAPHICS::GRasterUpdateFlags::ALIGN_X_LEFT
		| GW::GRAPHICS::GRasterUpdateFlags::ALIGN_X_CENTER
		| GW::GRAPHICS::GRasterUpdateFlags::ALIGN_X_RIGHT;
	const unsigned int bitmaskAlignY =
		GW::GRAPHICS::GRasterUpdateFlags::ALIGN_Y_TOP
		| GW::GRAPHICS::GRasterUpdateFlags::ALIGN_Y_CENTER
		| GW::GRAPHICS::GRasterUpdateFlags::ALIGN_Y_BOTTOM;
	const unsigned int bitmaskUpscale =
		GW::GRAPHICS::GRasterUpdateFlags::UPSCALE_2X
		| GW::GRAPHICS::GRasterUpdateFlags::UPSCALE_3X
		| GW::GRAPHICS::GRasterUpdateFlags::UPSCALE_4X
		| GW::GRAPHICS::GRasterUpdateFlags::UPSCALE_8X
		| GW::GRAPHICS::GRasterUpdateFlags::UPSCALE_16X
		| GW::GRAPHICS::GRasterUpdateFlags::STRETCH_TO_FIT;
	const unsigned int bitmaskInterpolate =
		GW::GRAPHICS::GRasterUpdateFlags::INTERPOLATE_NEAREST
		| GW::GRAPHICS::GRasterUpdateFlags::INTERPOLATE_BILINEAR;

	// The below operations are courtesy of Gateware.h, see MIT license for details
	// Returns the state of a single bit in a bitfield
	inline bool IsolateBit(unsigned int _flags, unsigned short _bit) {
		return (_flags >> _bit) & 1;
	}
	// check the presentation style for valid flag combinations
	bool ValidatePresentFlags();

	// compute the overlay scale and offset based on the present style flags
	void ComputeOverlayScaleAndOffset(VkViewport& _viewport, VkRect2D& _scissor);

	std::string ReadFileIntoString(const char* filePath);

public:
	// Constructor
	// IMPORTANT: You must use GW::GRAPHICS::GRasterUpdateFlags to edit the present style
	// By default it will place the overlay in the screen center with no scaling or interpolation
	Overlay(unsigned int _width, unsigned int _height, 
		GW::SYSTEM::GWindow _window, GW::GRAPHICS::GVulkanSurface _surface,
		unsigned int _presentStyle);

	// Lock the overlay for update, you can call this on a separate thread for better performance
	bool LockForUpdate(unsigned int _pixelCount, unsigned int** _outARGBPixels);

	// Unlock the overlay when you are done updating so it can be transferred to the GPU
	bool Unlock();

	// Transfer the overlay to the GPU
	// This must be done on the main rendering thread, *BEFORE* GVulkanSurface::StartFrame()
	bool TransferOverlay();

	// Render the latest overlay to the active swap chain image
	// This must be done on the main rendering thread, 
	// *BETWEEN* GVulkanSurface::StartFrame() and GVulkanSurface::EndFrame()
	bool RenderOverlay();

	~Overlay();

	// disable copy/move operations (unsafe)
	Overlay(const Overlay&) = delete;
	Overlay(Overlay&&) = delete;
	Overlay& operator=(const Overlay&) = delete;
	Overlay& operator=(Overlay&&) = delete;
};

// private implementation
void Overlay::CompileShaders()
{
	// Initialize runtime shader compiler HLSL -> SPIRV
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	shaderc_compile_options_t options = CreateCompileOptions();

	CompileVertexShader(compiler, options);
	CompileFragmentShader(compiler, options);

	// Free runtime shader compiler resources
	shaderc_compile_options_release(options);
	shaderc_compiler_release(compiler);
}

shaderc_compile_options_t Overlay::CreateCompileOptions()
{
	shaderc_compile_options_t retval = shaderc_compile_options_initialize();
	shaderc_compile_options_set_source_language(retval, shaderc_source_language_hlsl);
	shaderc_compile_options_set_invert_y(retval, false);
#ifndef NDEBUG
	shaderc_compile_options_set_generate_debug_info(retval);
#endif
	return retval;
}

void Overlay::CompileVertexShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
{
	std::string vertexShaderSource = ReadFileIntoString("../Shaders/OverlayVertex.hlsl");

	shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
		compiler, vertexShaderSource.c_str(), vertexShaderSource.length(),
		shaderc_vertex_shader, "main.vert", "main", options);

	if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
	{
		std::cout << "Vertex Shader Errors: \n" << shaderc_result_get_error_message(result) << std::endl;
		abort(); //Vertex shader failed to compile! 
		return;
	}

	GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
		(char*)shaderc_result_get_bytes(result), &vertexShader);

	shaderc_result_release(result); // done
}

std::string Overlay::ReadFileIntoString(const char* filePath)
{
	std::string output;
	unsigned int stringLength = 0;
	GW::SYSTEM::GFile file;

	file.Create();
	file.GetFileSize(filePath, stringLength);

	if (stringLength > 0 && +file.OpenBinaryRead(filePath))
	{
		output.resize(stringLength);
		file.Read(&output[0], stringLength);
	}
	else
		std::cout << "ERROR: File \"" << filePath << "\" Not Found!" << std::endl;

	return output;
}

void Overlay::CompileFragmentShader(const shaderc_compiler_t& compiler, const shaderc_compile_options_t& options)
{
	// 100% Switch to using the fragment shader for the PBR model
	std::string fragmentShaderSource = ReadFileIntoString("../Shaders/OverlayFragment.hlsl");

	shaderc_compilation_result_t result = shaderc_compile_into_spv( // compile
		compiler, fragmentShaderSource.c_str(), fragmentShaderSource.length(),
		shaderc_fragment_shader, "main.frag", "main", options);

	if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) // errors?
	{
		std::cout << "Fragment Shader Errors: \n" << shaderc_result_get_error_message(result) << std::endl;
		abort(); //Fragment shader failed to compile! 
		return;
	}

	GvkHelper::create_shader_module(device, shaderc_result_get_length(result), // load into Vulkan
		(char*)shaderc_result_get_bytes(result), &fragmentShader);

	shaderc_result_release(result); // done
}

// Cleanup
void Overlay::Cleanup()
{
	if (shutdown) {
		// lock for synchronous writes
		lock.LockSyncWrite();
		// wait for the device to finish
		vkDeviceWaitIdle(device);
		// destroy the sampler
		vkDestroySampler(device, sampler, nullptr);
		// destroy the pipeline layout
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		// destroy the descriptor pool
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		// destroy the descriptor set layout
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		// destroy the pipeline
		vkDestroyPipeline(device, pipeline, nullptr);
		// destroy the fragment shader
		vkDestroyShaderModule(device, fragmentShader, nullptr);
		// destroy the vertex shader
		vkDestroyShaderModule(device, vertexShader, nullptr);
		// destroy the overlay images
		for (auto& overlayImage : overlayImages) {
			vkDestroyImageView(device, overlayImage.imageView, nullptr);
			vkDestroyImage(device, overlayImage.image, nullptr);
			vkFreeMemory(device, overlayImage.memory, nullptr);
		}
		// destroy the staging buffer
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
		// unlock the overlay
		lock.UnlockSyncWrite();
	}
	// unregister the listener
	shutdown = nullptr;
}

// check the presentation style for valid flag combinations
bool Overlay::ValidatePresentFlags()
{
	unsigned int testFlags = 0;
	// validate x alignment flags
	testFlags = presentStyle & bitmaskAlignX;
	if ((IsolateBit(testFlags, UpdateFlagBitPosition::BIT_ALIGN_X_LEFT)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_ALIGN_X_CENTER)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_ALIGN_X_RIGHT)
		) > 1)
	{
		return false;
	}
	// validate y alignment flags
	testFlags = presentStyle & bitmaskAlignY;
	if ((IsolateBit(testFlags, UpdateFlagBitPosition::BIT_ALIGN_Y_TOP)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_ALIGN_Y_CENTER)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_ALIGN_Y_BOTTOM)
		) > 1)
	{
		return false;
	}
	// validate upscaling flags
	testFlags = presentStyle & bitmaskUpscale;
	if ((IsolateBit(testFlags, UpdateFlagBitPosition::BIT_UPSCALE_2X)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_UPSCALE_3X)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_UPSCALE_4X)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_UPSCALE_8X)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_UPSCALE_16X)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_STRETCH_TO_FIT)
		) > 1)
	{
		return false;
	}
	// validate interpolation flags
	testFlags = presentStyle & bitmaskInterpolate;
	if ((IsolateBit(testFlags, UpdateFlagBitPosition::BIT_INTERPOLATE_NEAREST)
		+ IsolateBit(testFlags, UpdateFlagBitPosition::BIT_INTERPOLATE_BILINEAR)
		) > 1)
	{
		return false;
	}
	return true;
}

// compute the overlay scale and offset based on the present style flags
void Overlay::ComputeOverlayScaleAndOffset(VkViewport& _viewport, VkRect2D& _scissor)
{
	// grab current window dimensions
	unsigned int client_width = 0;
	unsigned int client_height = 0;
	windowHandle.GetClientWidth(client_width);
	windowHandle.GetClientHeight(client_height);

	unsigned int testFlags = 0;
	// store surface dimensions and border color locally to avoid needing to read data more than once
	unsigned int upscaledDataWidth = width;
	unsigned int upscaledDataHeight = height;
	// determine data dimensions after processing
	testFlags = presentStyle & bitmaskUpscale;
	switch (testFlags)
	{
	case GW::GRAPHICS::UPSCALE_2X:
		upscaledDataWidth = width << 1;
		upscaledDataHeight = height << 1;
		break;
	case GW::GRAPHICS::UPSCALE_3X:
		upscaledDataWidth = width * 3;
		upscaledDataHeight = height * 3;
		break;
	case GW::GRAPHICS::UPSCALE_4X:
		upscaledDataWidth = width << 2;
		upscaledDataHeight = height << 2;
		break;
	case GW::GRAPHICS::UPSCALE_8X:
		upscaledDataWidth = width << 3;
		upscaledDataHeight = height << 3;
		break;
	case GW::GRAPHICS::UPSCALE_16X:
		upscaledDataWidth = width << 4;
		upscaledDataHeight = height << 4;
		break;
	case GW::GRAPHICS::STRETCH_TO_FIT:
		upscaledDataWidth = client_width;
		upscaledDataHeight = client_height;
		break;
	default:
		upscaledDataWidth = width;
		upscaledDataHeight = height;
		break;
	}
	// calculate pixel coordinate scaling ratios
	overlayConstants.scale[0] = width / static_cast<float>(upscaledDataWidth);
	overlayConstants.scale[1] = height / static_cast<float>(upscaledDataHeight);
	// determine X alignment
	testFlags = presentStyle & bitmaskAlignX;
	switch (testFlags)
	{
	case GW::GRAPHICS::ALIGN_X_LEFT:
		overlayConstants.offset[0] = 0;
		break;
	case GW::GRAPHICS::ALIGN_X_RIGHT:
		overlayConstants.offset[0] = static_cast<int>(client_width) -
			static_cast<int>(upscaledDataWidth);
		break;
	case GW::GRAPHICS::ALIGN_X_CENTER:
	default:
		overlayConstants.offset[0] = (static_cast<int>(client_width) -
			static_cast<int>(upscaledDataWidth)) >> 1;
		break;
	}
	// determine Y alignment
	testFlags = presentStyle & bitmaskAlignY;
	switch (testFlags)
	{
	case GW::GRAPHICS::ALIGN_Y_TOP:
		overlayConstants.offset[1] = 0;
		break;
	case GW::GRAPHICS::ALIGN_Y_BOTTOM:
		overlayConstants.offset[1] = static_cast<int>(client_height) -
			static_cast<int>(upscaledDataHeight);
		break;
	case GW::GRAPHICS::ALIGN_Y_CENTER:
	default:
		overlayConstants.offset[1] = (static_cast<int>(client_height) -
			static_cast<int>(upscaledDataHeight)) >> 1;
		break;
	}
	// viewport defines NDC to cover all of the window for pixel based sampling
	_viewport.x = 0.0f;
	_viewport.y = 0.0f;
	_viewport.width = static_cast<float>(client_width);
	_viewport.height = static_cast<float>(client_height);
	_viewport.minDepth = 0.0f;
	_viewport.maxDepth = 1.0f;
	// use computed dimensions to limit the area rendered in Vulkan (boost performance)
	_scissor.offset = {
		G_CLAMP(static_cast<int>(overlayConstants.offset[0]), 0, static_cast<int>(client_width)),
		G_CLAMP(static_cast<int>(overlayConstants.offset[1]), 0, static_cast<int>(client_height)) };
	_scissor.extent = {
		G_CLAMP(static_cast<unsigned int>(overlayConstants.offset[0])
			+ upscaledDataWidth, 0, client_width),
		G_CLAMP(static_cast<unsigned int>(overlayConstants.offset[1])
			+ upscaledDataHeight, 0, client_height) };

	// perform inverse operations to get the overlay to render in the correct location
	overlayConstants.offset[0] *= -1.0f;
	overlayConstants.offset[1] *= -1.0f;
	overlayConstants.scale[0] /= 1.0f; // reciprocal of the scale
	overlayConstants.scale[1] /= 1.0f; // reciprocal of the scale
}

// public implementation
Overlay::Overlay(unsigned int _width, unsigned int _height,
	GW::SYSTEM::GWindow _window, GW::GRAPHICS::GVulkanSurface _surface,
	unsigned int _presentStyle) : width(_width), height(_height),
		windowHandle(_window), surfaceHandle(_surface), presentStyle(_presentStyle)
{
	// validate the present style flags
	if (!ValidatePresentFlags()) {
		throw std::runtime_error("Invalid present style flags");
	}

	// get the Vulkan handles
	surfaceHandle.GetDevice((void**)&device);
	surfaceHandle.GetPhysicalDevice((void**)&physicalDevice);
	surfaceHandle.GetRenderPass((void**)&renderPass);
	surfaceHandle.GetCommandPool((void**)&commandPool);
	surfaceHandle.GetGraphicsQueue((void**)&graphicsQueue);

	// create CPU synchronization primitive
	lock.Create();
	// start the overlay update count at 0
	overlayUpdateCount = 0;

	// Load Shaders for Overlay (Replace with SPV headers when done)
	CompileShaders();

	// create the overlay images
	unsigned int maxFrames = 0;
	surfaceHandle.GetSwapchainImageCount(maxFrames);
	
	//// create the vertex and fragment shaders
	// // saving this approach for when the shaders are pre-compiled as SPV headers
	//{
	//	// vertex shader
	//	{
	//		std::vector<char> vertexShaderCode = GW::SYSTEM::GFile::ReadFile("overlay.vert.spv");
	//		VkShaderModuleCreateInfo createInfo = {};
	//		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	//		createInfo.codeSize = vertexShaderCode.size();
	//		createInfo.pCode = reinterpret_cast<const uint32_t*>(vertexShaderCode.data());
	//		vkCreateShaderModule(device, &createInfo, nullptr, &vertexShader);
	//	}
	//	// fragment shader
	//	{
	//		std::vector<char> fragmentShaderCode = GW::SYSTEM::GFile::ReadFile("overlay.frag.spv");
	//		VkShaderModuleCreateInfo createInfo = {};
	//		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	//		createInfo.codeSize = fragmentShaderCode.size();
	//		createInfo.pCode = reinterpret_cast<const uint32_t*>(fragmentShaderCode.data());
	//		vkCreateShaderModule(device, &createInfo, &fragmentShader);
	//	}
	//}
	
	// create the sampler
	{
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_NEAREST; // default to nearest
		samplerInfo.minFilter = VK_FILTER_NEAREST; // default to nearest
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_TRUE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // default to nearest
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;
		// switch sampler to linear interpolation if requested
		if (presentStyle & GW::GRAPHICS::GRasterUpdateFlags::INTERPOLATE_BILINEAR) {
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
		vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
	}
	// create the descriptor set layout
	{
		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 0;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = &sampler;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &samplerLayoutBinding;

		vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
	}
	// create the descriptor pool
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = maxFrames;

		vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
	}
	// create the pipeline layout
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		// setup push constants for the overlay
		VkPushConstantRange pushConstantRange = {};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(OverlayConstants);
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

		vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
	}
	// create the pipeline
	{
		VkPipelineShaderStageCreateInfo shaderStages[2] = {};
		shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStages[0].module = vertexShader;
		shaderStages[0].pName = "main";
		shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages[1].module = fragmentShader;
		shaderStages[1].pName = "main";
		
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(_width);
		viewport.height = static_cast<float>(_height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = { _width, _height };

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		// depth stencil state
		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_FALSE;
		depthStencil.depthWriteEnable = VK_FALSE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = VK_NULL_HANDLE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;

		vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	}

	// allocate one overlay image for each swap chain image and link to descriptor set
	overlayImages.resize(maxFrames);
	for (auto& overlayImage : overlayImages) {
		overlayImage.lastUpdate = 0;
		// allocate one BGRA image for each overlay image
		VkExtent3D tempExtent = { _width, _height, 1 };
		GvkHelper::create_image(physicalDevice, device, tempExtent, 1,
			VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr, &overlayImage.image, &overlayImage.memory);

		// transition the image layout for optimal CPU transfers
		// we provide our own as the one in GvkHelper does not support the VK_IMAGE_LAYOUT_GENERAL layout
		VkCommandBuffer transition_buffer = VK_NULL_HANDLE;
		GvkHelper::signal_command_start(device, commandPool, &transition_buffer);
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // best for CPU transfers
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = overlayImage.image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		vkCmdPipelineBarrier(transition_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &transition_buffer);

		// create an image view for the overlay image
		GvkHelper::create_image_view(device, overlayImage.image, VK_FORMAT_B8G8R8A8_SRGB,
			VK_IMAGE_ASPECT_COLOR_BIT, 1, nullptr, &overlayImage.imageView);

		// create a descriptor set for the overlay image
		VkDescriptorSetAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &descriptorSetLayout;

		vkAllocateDescriptorSets(device, &allocateInfo, &overlayImage.descriptorSet);
		// update the descriptor set
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.imageView = overlayImage.imageView;
		imageInfo.sampler = nullptr; // using immutable sampler

		VkWriteDescriptorSet descriptorWrite = {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = overlayImage.descriptorSet;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}

	// allocate CPU staging buffer
	GvkHelper::create_buffer(physicalDevice, device, _width* _height * 4,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);

	// GVulkanSurface will inform us when to release any allocated resources
	shutdown.Create([&](const GW::GEvent& g) {
		GW::GRAPHICS::GVulkanSurface::Events event;
		if (+g.Read(event)) {
			if (event == GW::GRAPHICS::GVulkanSurface::Events::RELEASE_RESOURCES) {
				Cleanup(); // unlike D3D we must be careful about destroy timing
			}
		}
	});
	surfaceHandle.Register(shutdown);
}

// render implementation
bool Overlay::LockForUpdate(unsigned int _pixelCount, unsigned int** _outARGBPixels)
{
	// ensure pixel count is correct
	if (_pixelCount != width * height) {
		return false;
	}
	// lock for synchronous writes
	lock.LockSyncWrite();

	// if we have shutdown, we cannot update the overlay
	if (shutdown == nullptr) {
		lock.UnlockSyncWrite();
		return false;
	}

	// continue to update the staging buffer
	vkMapMemory(device, stagingBufferMemory, 0, 
		_pixelCount << 2, 0, reinterpret_cast<void**>(_outARGBPixels));
	
	return true;
}

bool Overlay::Unlock()
{
	if (shutdown == nullptr) {
		lock.UnlockSyncWrite();
		return false;
	}

	vkUnmapMemory(device, stagingBufferMemory);

	++overlayUpdateCount;

	// done with the overlay update
	lock.UnlockSyncWrite();

	return true;
}

// transfer implementation
bool Overlay::TransferOverlay()
{
	if (shutdown == nullptr) {
		return false;
	}
	// with the staging buffer updated, we can now copy the data to the overlay image
	// find the overlay image to update
	unsigned int currentImageIndex;
	surfaceHandle.GetSwapchainCurrentImage(currentImageIndex);
	OverlayImage& overlayImage = overlayImages[currentImageIndex];

	// only update the overlay image if it has changed
	if (overlayImage.lastUpdate == overlayUpdateCount) {
		return true;
	}

	//Command Buffer Allocate Info
	VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
	command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_allocate_info.commandPool = commandPool;
	command_buffer_allocate_info.commandBufferCount = 1;

	//Create the command buffer and allocate the command buffer with create info
	VkCommandBuffer transfer_buffer = VK_NULL_HANDLE;
	vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &transfer_buffer);

	//Start the command buffer's create info
	VkCommandBufferBeginInfo command_buffer_begin_info = {};
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	//Begin the Command Buffer's recording process
	vkBeginCommandBuffer(transfer_buffer, &command_buffer_begin_info);

	// copy the staging buffer to the overlay image
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		//VkCommandBuffer transfer_buffer = VK_NULL_HANDLE;
		//GvkHelper::signal_command_start(device, commandPool, &transfer_buffer);
		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(transfer_buffer, stagingBuffer, overlayImage.image,
			VK_IMAGE_LAYOUT_GENERAL, 1, &region);
		//GvkHelper::signal_command_end(device, graphicsQueue, commandPool, &transfer_buffer);
	}

	//End the command buffer's recording process
	vkEndCommandBuffer(transfer_buffer);
	//Submit the command buffer to the graphics queue
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transfer_buffer;
	
	// force a CPU sync until staging buffer is transferred
	lock.LockAsyncRead();

		//Submit the command buffer to the graphics queue
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		//Wait for the queue to finish
		vkQueueWaitIdle(graphicsQueue); // switch for maxFrame pre-computed command buffers

	// release the CPU sync
	lock.UnlockAsyncRead();

	//Free the command buffer
	vkFreeCommandBuffers(device, commandPool, 1, &transfer_buffer); // can move to clean up

	// update the last update count
	overlayImage.lastUpdate = overlayUpdateCount;

	return true;
}

// render implementation
bool Overlay::RenderOverlay()
{
	// find the overlay image to draw
	unsigned int currentImageIndex;
	surfaceHandle.GetSwapchainCurrentImage(currentImageIndex);
	OverlayImage& overlayImage = overlayImages[currentImageIndex];

	// acquire command buffer
	VkCommandBuffer commandBuffer;
	surfaceHandle.GetCommandBuffer(currentImageIndex, reinterpret_cast<void**>(&commandBuffer));

	// render the overlay image
	{
		// bind the pipeline
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		// bind the descriptor set
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
			pipelineLayout, 0, 1, &overlayImage.descriptorSet, 0, nullptr);

		// calculate the UV offset and scale based on presentation style
		VkViewport viewport = {};
		VkRect2D scissor = {};
		ComputeOverlayScaleAndOffset(viewport, scissor);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// push the constants
		vkCmdPushConstants(commandBuffer, pipelineLayout, 
			VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(OverlayConstants), &overlayConstants);

		// draw the overlay
		vkCmdDraw(commandBuffer, 4, 1, 0, 0);
	}
	return true;
}

// free all resources
Overlay::~Overlay()
{
	Cleanup();
}
