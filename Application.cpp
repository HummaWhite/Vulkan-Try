#include "Application.h"

#if defined(_DEBUG)
const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
#else
const std::vector<const char*> validationLayers;
#endif

const std::vector<const char*> deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const int MAX_FRAMES_IN_FLIGHT = 2;

Application::Application(const std::string& name, int width, int height):
	appName(name), windowWidth(width), windowHeight(height)
{
	setupWindow();
	setupVulkan();
}

Application::~Application()
{
	cleanUp();
}

int Application::run()
{
	while (!shouldTerminate && !glfwWindowShouldClose(window))
	{
		mainLoop();
	}

	device.waitIdle();
	return 0;
}

void Application::setupWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(windowWidth, windowHeight, appName.c_str(), nullptr, nullptr);
}

void Application::setupVulkan()
{
	createInstance();
	createSurface();
	selectPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
	createCommandBuffers();
	createSyncObjects();
}

void Application::mainLoop()
{
	glfwPollEvents();
	drawFrame();
}

void Application::cleanUp()
{
	for (auto& semaphore : renderFinishedSemaphores)
		device.destroySemaphore(semaphore);
	for (auto& semaphore : imageAvailableSemaphores)
		device.destroySemaphore(semaphore);
	for (auto& fence : inFlightFences)
		device.destroyFence(fence);

	device.destroyCommandPool(commandPool);

	for (auto framebuffer : swapchainFramebuffers)
		device.destroyFramebuffer(framebuffer);

	device.destroyPipeline(graphicsPipeline);
	device.destroyPipelineLayout(pipelineLayout);
	device.destroyRenderPass(renderPass);

	for (auto imageView : swapchainImageViews)
		device.destroyImageView(imageView);

	device.destroySwapchainKHR(swapchain);
	device.destroy();

	instance.destroySurfaceKHR(surface);
	instance.destroy();

	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::createInstance()
{
	auto appInfo = vk::ApplicationInfo()
		.setPApplicationName(appName.c_str())
		.setApplicationVersion(1)
		.setPEngineName("None")
		.setEngineVersion(1)
		.setApiVersion(VK_API_VERSION_1_0);

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::cout << "[GLFW Vulkan Extentions]\n";
	for (uint32_t i = 0; i < glfwExtensionCount; i++)
		std::cout << "\t--" << glfwExtensions[i] << "\n";
	std::cout << "\n";

	auto createInfo = vk::InstanceCreateInfo()
		.setFlags(vk::InstanceCreateFlags())
		.setPApplicationInfo(&appInfo)
		.setEnabledExtensionCount(glfwExtensionCount)
		.setPpEnabledExtensionNames(glfwExtensions)
		.setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()))
		.setPpEnabledLayerNames(validationLayers.data());
	try
	{
		instance = vk::createInstance(createInfo);
	}
	catch (const std::exception& e)
	{
		std::cout << "[Error] " << e.what() << std::endl;
		shouldTerminate = true;
	}
}

void Application::createSurface()
{
	if (glfwCreateWindowSurface((VkInstance)instance, window, nullptr, (VkSurfaceKHR*)&surface) != VK_SUCCESS)
	{
		throw std::runtime_error("[Error] Failed to create window surface");
	}
}

void Application::selectPhysicalDevice()
{
	auto devices = instance.enumeratePhysicalDevices();
	std::cout << "[Number of Physical Devices] " << devices.size() << "\n";

	for (const auto& i : devices)
	{
		if (isDeviceAvailable(i))
		{
			physicalDevice = i;
			break;
		}
	}
	std::cout << "\n";
	if (physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("[Error] Failed to find any available physical device");
}

void Application::createLogicalDevice()
{
	auto queueFamilyIndices = findQueueFamilies(physicalDevice);
	if (!queueFamilyIndices.has_value())
	{
		throw std::runtime_error("[Error] Failed to find any available queue family");
	}
	uint32_t graphicsFamily = queueFamilyIndices.value().first;
	uint32_t presentFamily = queueFamilyIndices.value().second;

	float queuePriority = 1.0f;

	uint32_t uniqueQueueFamilies[] = { graphicsFamily, presentFamily };
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

	for (auto queueFamily : uniqueQueueFamilies)
	{
		vk::DeviceQueueCreateInfo queueCreateInfo = vk::DeviceQueueCreateInfo()
			.setQueueFamilyIndex(queueFamily)
			.setQueueCount(1)
			.setPQueuePriorities(&queuePriority);
		queueCreateInfos.push_back(queueCreateInfo);
	}

	vk::PhysicalDeviceFeatures deviceFeatures;

	auto createInfo = vk::DeviceCreateInfo()
		.setPQueueCreateInfos(queueCreateInfos.data())
		.setQueueCreateInfoCount(1)
		.setPEnabledFeatures(&deviceFeatures)
		.setEnabledExtensionCount(static_cast<uint32_t>(deviceExtensions.size()))
		.setPpEnabledExtensionNames(deviceExtensions.data())
		.setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()))
		.setPpEnabledLayerNames(validationLayers.data());
	try
	{
		device = physicalDevice.createDevice(createInfo);
	}
	catch (const std::exception& e)
	{
		std::cout << "[Error] " << e.what() << "\n";
		shouldTerminate = true;
	}

	graphicsQueue = device.getQueue(graphicsFamily, 0);
	presentQueue = device.getQueue(presentFamily, 0);
}

void Application::createSwapchain()
{
	auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
	auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

	auto surfaceFormat = selectSwapchainSurfaceFormat(formats);
	auto presentMode = selectSwapchainPresentMode(presentModes);
	auto extent = selectSwapchainExtent(capabilities);

	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0)
		imageCount = std::min(imageCount, capabilities.maxImageCount);

	auto indices = findQueueFamilies(physicalDevice);
	uint32_t graphicsFamilyIndex = indices.value().first;
	uint32_t presentFamilyIndex = indices.value().second;
	uint32_t queueFamilyIndices[] = { graphicsFamilyIndex, presentFamilyIndex };
	bool identical = (graphicsFamilyIndex == presentFamilyIndex);

	auto createInfo = vk::SwapchainCreateInfoKHR()
		.setSurface(surface)
		.setMinImageCount(imageCount)
		.setImageFormat(surfaceFormat.format)
		.setImageColorSpace(surfaceFormat.colorSpace)
		.setImageExtent(extent)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setImageSharingMode(identical ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent)
		.setQueueFamilyIndexCount(identical ? 0 : 2)
		.setPQueueFamilyIndices(identical ? nullptr : queueFamilyIndices)
		.setPreTransform(capabilities.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(presentMode)
		.setClipped(VK_TRUE)
		.setOldSwapchain(VK_NULL_HANDLE);
	try
	{
		swapchain = device.createSwapchainKHR(createInfo);
	}
	catch (const std::exception& e)
	{
		std::cout << "[Error] " << e.what() << "\n";
		shouldTerminate = true;
		return;
	}

	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;
	swapchainImages = device.getSwapchainImagesKHR(swapchain);
}

void Application::createImageViews()
{
	auto components = vk::ComponentMapping()
		.setR(vk::ComponentSwizzle::eIdentity)
		.setG(vk::ComponentSwizzle::eIdentity)
		.setB(vk::ComponentSwizzle::eIdentity)
		.setA(vk::ComponentSwizzle::eIdentity);
	
	auto subresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(0)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(1);

	for (const auto& image : swapchainImages)
	{
		auto createInfo = vk::ImageViewCreateInfo()
			.setImage(image)
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(swapchainImageFormat)
			.setComponents(components)
			.setSubresourceRange(subresourceRange);

		auto imageView = device.createImageView(createInfo);
		swapchainImageViews.push_back(imageView);
	}
}

void Application::createRenderPass()
{
	auto colorAttachment = vk::AttachmentDescription()
		.setFormat(swapchainImageFormat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	auto colorAttachmentRef = vk::AttachmentReference()
		.setAttachment(0)
		.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

	auto subpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachmentCount(1)
		.setPColorAttachments(&colorAttachmentRef);

	auto dependency = vk::SubpassDependency()
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setDstSubpass(0)
		.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlags())
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

	auto renderPassInfo = vk::RenderPassCreateInfo()
		.setAttachmentCount(1)
		.setPAttachments(&colorAttachment)
		.setSubpassCount(1)
		.setPSubpasses(&subpass)
		.setDependencyCount(1)
		.setPDependencies(&dependency);

	renderPass = device.createRenderPass(renderPassInfo);
}

void Application::createGraphicsPipeline()
{
	auto vsModule = createShaderModule("res/shaders/helloVK_vs.spv");
	auto fsModule = createShaderModule("res/shaders/helloVK_fs.spv");

	auto vsStageInfo = vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eVertex)
		.setModule(vsModule)
		.setPName("main");

	auto fsStageInfo = vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eFragment)
		.setModule(fsModule)
		.setPName("main");

	vk::PipelineShaderStageCreateInfo shaderStages[] = { vsStageInfo, fsStageInfo };

	auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
		.setVertexBindingDescriptionCount(0)
		.setPVertexBindingDescriptions(nullptr)
		.setVertexAttributeDescriptionCount(0)
		.setPVertexAttributeDescriptions(nullptr);

	auto inputAssemblyState = vk::PipelineInputAssemblyStateCreateInfo()
		.setTopology(vk::PrimitiveTopology::eTriangleList)
		.setPrimitiveRestartEnable(VK_FALSE);

	auto viewport = vk::Viewport()
		.setX(0.0f)
		.setY(0.0f)
		.setWidth(swapchainExtent.width)
		.setHeight(swapchainExtent.height)
		.setMinDepth(0.0f)
		.setMaxDepth(1.0f);

	auto scissor = vk::Rect2D()
		.setOffset({ 0, 0 })
		.setExtent(swapchainExtent);

	auto viewportState = vk::PipelineViewportStateCreateInfo()
		.setViewportCount(1)
		.setPViewports(&viewport)
		.setScissorCount(1)
		.setPScissors(&scissor);

	auto rasterizationState = vk::PipelineRasterizationStateCreateInfo()
		.setDepthClampEnable(VK_FALSE)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setLineWidth(1.0f)
		.setCullMode(vk::CullModeFlagBits::eBack)
		.setFrontFace(vk::FrontFace::eClockwise)
		.setDepthBiasEnable(VK_FALSE)
		.setDepthBiasConstantFactor(0.0f)
		.setDepthBiasClamp(0.0f)
		.setDepthBiasSlopeFactor(0.0f);

	auto multisampleState = vk::PipelineMultisampleStateCreateInfo()
		.setSampleShadingEnable(VK_FALSE)
		.setRasterizationSamples(vk::SampleCountFlagBits::e1)
		.setMinSampleShading(1.0f)
		.setPSampleMask(nullptr)
		.setAlphaToCoverageEnable(VK_FALSE)
		.setAlphaToOneEnable(VK_FALSE);

	// depth / stencil

	auto colorBlendAttachmentState = vk::PipelineColorBlendAttachmentState()
		.setColorWriteMask(
			vk::ColorComponentFlagBits::eR |
			vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB |
			vk::ColorComponentFlagBits::eA)
		.setBlendEnable(VK_FALSE)
		.setSrcColorBlendFactor(vk::BlendFactor::eOne)
		.setDstColorBlendFactor(vk::BlendFactor::eZero)
		.setColorBlendOp(vk::BlendOp::eAdd)
		.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
		.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
		.setAlphaBlendOp(vk::BlendOp::eAdd);

	auto colorBlendState = vk::PipelineColorBlendStateCreateInfo()
		.setLogicOpEnable(VK_FALSE)
		.setLogicOp(vk::LogicOp::eCopy)
		.setAttachmentCount(1)
		.setPAttachments(&colorBlendAttachmentState)
		.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });

	vk::DynamicState dynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eLineWidth };
	auto dynamicState = vk::PipelineDynamicStateCreateInfo()
		.setDynamicStateCount(2)
		.setPDynamicStates(dynamicStates);

	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayoutCount(0)
		.setPSetLayouts(nullptr)
		.setPushConstantRangeCount(0)
		.setPPushConstantRanges(nullptr);

	pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

	auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
		.setStageCount(2)
		.setPStages(shaderStages)
		.setPVertexInputState(&vertexInputInfo)
		.setPInputAssemblyState(&inputAssemblyState)
		.setPViewportState(&viewportState)
		.setPRasterizationState(&rasterizationState)
		.setPMultisampleState(&multisampleState)
		.setPDepthStencilState(nullptr)
		.setPColorBlendState(&colorBlendState)
		.setPDynamicState(nullptr)
		.setLayout(pipelineLayout)
		.setRenderPass(renderPass)
		.setSubpass(0)
		.setBasePipelineHandle(VK_NULL_HANDLE)
		.setBasePipelineIndex(-1);

	graphicsPipeline = device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo).value;

	device.destroyShaderModule(vsModule);
	device.destroyShaderModule(fsModule);
}

void Application::createFramebuffers()
{
	for (const auto& imageView : swapchainImageViews)
	{
		auto framebufferInfo = vk::FramebufferCreateInfo()
			.setRenderPass(renderPass)
			.setAttachmentCount(1)
			.setPAttachments(&imageView)
			.setWidth(swapchainExtent.width)
			.setHeight(swapchainExtent.height)
			.setLayers(1);

		auto framebuffer = device.createFramebuffer(framebufferInfo);
		swapchainFramebuffers.push_back(framebuffer);
	}
}

void Application::createCommandPool()
{
	auto graphicsFamily = findQueueFamilies(physicalDevice).value().first;

	auto commandPoolInfo = vk::CommandPoolCreateInfo()
		.setQueueFamilyIndex(graphicsFamily);

	commandPool = device.createCommandPool(commandPoolInfo);
}

void Application::createCommandBuffers()
{
	auto allocInfo = vk::CommandBufferAllocateInfo()
		.setCommandPool(commandPool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(swapchainFramebuffers.size());

	commandBuffers = device.allocateCommandBuffers(allocInfo);

	for (int i = 0; i < commandBuffers.size(); i++)
	{
		auto& commandBuffer = commandBuffers[i];
		auto beginInfo = vk::CommandBufferBeginInfo();
		commandBuffer.begin(beginInfo);

		auto renderArea = vk::Rect2D()
			.setOffset({ 0, 0 })
			.setExtent(swapchainExtent);

		auto clearColor = vk::ClearValue()
			.setColor(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });

		auto renderPassBeginInfo = vk::RenderPassBeginInfo()
			.setRenderPass(renderPass)
			.setFramebuffer(swapchainFramebuffers[i])
			.setRenderArea(renderArea)
			.setClearValueCount(1)
			.setPClearValues(&clearColor);

		commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.endRenderPass();
		commandBuffer.end();
	}
}

void Application::createSyncObjects()
{
	auto semaphoreInfo = vk::SemaphoreCreateInfo();
	auto fenceInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto availableSemaphore = device.createSemaphore(semaphoreInfo);
		auto finishedSemaphore = device.createSemaphore(semaphoreInfo);
		auto fence = device.createFence(fenceInfo);
		imageAvailableSemaphores.push_back(availableSemaphore);
		renderFinishedSemaphores.push_back(finishedSemaphore);
		inFlightFences.push_back(fence);
	}

	imagesInFlight.resize(swapchainImages.size(), VK_NULL_HANDLE);
}

void Application::drawFrame()
{
	device.waitForFences({ inFlightFences[currentFrame] }, VK_TRUE, UINT64_MAX);

	auto imageIndex = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE).value;
	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
		device.waitForFences({ inFlightFences[currentFrame] }, VK_TRUE, UINT64_MAX);

	imagesInFlight[imageIndex] = inFlightFences[currentFrame];
	
	vk::Semaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
	vk::Semaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	auto submitInfo = vk::SubmitInfo()
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(waitSemaphores)
		.setPWaitDstStageMask(waitStages)
		.setCommandBufferCount(1)
		.setPCommandBuffers(&commandBuffers[imageIndex])
		.setSignalSemaphoreCount(1)
		.setPSignalSemaphores(signalSemaphores);

	device.resetFences({ inFlightFences[currentFrame] });

	graphicsQueue.submit(1, &submitInfo, inFlightFences[currentFrame]);

	vk::SwapchainKHR swapchains[] = { swapchain };

	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(signalSemaphores)
		.setSwapchainCount(1)
		.setPSwapchains(swapchains)
		.setPImageIndices(&imageIndex);

	presentQueue.presentKHR(presentInfo);
	presentQueue.waitIdle();

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool Application::isDeviceAvailable(vk::PhysicalDevice device)
{
	auto deviceProperties = device.getProperties();
	auto deviceFeatures = device.getFeatures();
	auto queueFamilyIndices = findQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	auto capabilities = device.getSurfaceCapabilitiesKHR(surface);
	auto formats = device.getSurfaceFormatsKHR(surface);
	auto presentModes = device.getSurfacePresentModesKHR(surface);

	bool swapchainAdequate = !formats.empty() && !presentModes.empty();

	return deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
		deviceFeatures.geometryShader &&
		queueFamilyIndices.has_value() &&
		swapchainAdequate &&
		extensionsSupported;
}

std::optional<std::pair<uint32_t, uint32_t>> Application::findQueueFamilies(vk::PhysicalDevice device)
{
	auto queueFamilies = device.getQueueFamilyProperties();
	//std::cout << "\tDevice has " << queueFamilies.size() << " QueueFamilies\n";
	std::optional<std::pair<uint32_t, uint32_t>> ret;

	uint32_t i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
		{
			if (device.getSurfaceSupportKHR(i, surface))
			{
				ret = std::pair<uint32_t, uint32_t>(i, i);
				break;
			}
		}
		i++;
	}
	return ret;
}

bool Application::checkDeviceExtensionSupport(vk::PhysicalDevice device)
{
	auto availableExtensions = vk::enumerateInstanceExtensionProperties();
	std::cout << "[Vulkan Extensions Supported]\n";
	for (const auto& i : availableExtensions)
		std::cout << "\t--" << i.extensionName << "\n";

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	bool supported = true;
	std::cout << "[Vulkan Extensions required]\n";
	for (const auto& i : requiredExtensions)
	{
		std::cout << "\t--" << i;
		if (requiredExtensions.find(i) != requiredExtensions.end())
			std::cout << "  [Found]\n";
		else
		{
			supported = false;
			break;
		}
	}
	return supported;
}

vk::SurfaceFormatKHR Application::selectSwapchainSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats)
{
	for (const auto& format : formats)
	{
		if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			return format;
	}
	return formats[0];
}

vk::PresentModeKHR Application::selectSwapchainPresentMode(const std::vector<vk::PresentModeKHR>& modes)
{
	for (const auto& mode : modes)
	{
		if (mode == vk::PresentModeKHR::eMailbox)
			return mode;
	}
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Application::selectSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
		return capabilities.currentExtent;
	
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	auto actualExtent = vk::Extent2D()
		.setWidth(static_cast<uint32_t>(width))
		.setHeight(static_cast<uint32_t>(height));

	actualExtent.width = std::max(
		capabilities.minImageExtent.width,
		std::min(capabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = std::max(
		capabilities.minImageExtent.height,
		std::min(capabilities.maxImageExtent.height, actualExtent.height));

	return actualExtent;
}

std::vector<char> Application::readShader(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("[Error] Unable to read file: " + filename);

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> ret(fileSize);
	file.seekg(0);
	file.read(ret.data(), fileSize);
	file.close();

	return ret;
}

vk::ShaderModule Application::createShaderModule(const std::vector<char>& code)
{
	auto createInfo = vk::ShaderModuleCreateInfo()
		.setCodeSize(code.size())
		.setPCode(reinterpret_cast<const uint32_t*>(code.data()));

	return device.createShaderModule(createInfo);
}

vk::ShaderModule Application::createShaderModule(const std::string& filename)
{
	auto code = readShader(filename);
	return createShaderModule(code);
}
