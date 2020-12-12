#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <optional>
#include <fstream>
#include <vector>
#include <tuple>
#include <set>

class Application
{
public:
	Application(const std::string& name, int width, int height);
	~Application();
	int run();

private:
	void setupWindow();
	void setupVulkan();
	void mainLoop();
	void cleanUp();

	void createInstance();
	void createSurface();
	void selectPhysicalDevice();
	void createLogicalDevice();
	void createSwapchain();
	void createImageViews();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSyncObjects();

	void drawFrame();

	bool isDeviceAvailable(vk::PhysicalDevice device);
	std::optional<std::pair<uint32_t, uint32_t>> findQueueFamilies(vk::PhysicalDevice device);
	bool checkDeviceExtensionSupport(vk::PhysicalDevice device);

	vk::SurfaceFormatKHR selectSwapchainSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);
	vk::PresentModeKHR selectSwapchainPresentMode(const std::vector<vk::PresentModeKHR>& modes);
	vk::Extent2D selectSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

	std::vector<char> readShader(const std::string& filename);
	vk::ShaderModule createShaderModule(const std::vector<char>& code);
	vk::ShaderModule createShaderModule(const std::string& filename);

protected:
	int windowWidth;
	int windowHeight;
	bool shouldTerminate = false;
	GLFWwindow* window;

private:
	std::string appName;
	vk::Instance instance;

	vk::PhysicalDevice physicalDevice = VK_NULL_HANDLE;
	vk::Device device;

	vk::Queue graphicsQueue;
	vk::Queue presentQueue;

	vk::SurfaceKHR surface;

	vk::SwapchainKHR swapchain;
	vk::Format swapchainImageFormat;
	vk::Extent2D swapchainExtent;
	std::vector<vk::Image> swapchainImages;
	std::vector<vk::ImageView> swapchainImageViews;

	vk::PipelineLayout pipelineLayout;
	vk::RenderPass renderPass;
	vk::Pipeline graphicsPipeline;
	std::vector<vk::Framebuffer> swapchainFramebuffers;

	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffers;

	std::vector<vk::Semaphore> imageAvailableSemaphores;
	std::vector<vk::Semaphore> renderFinishedSemaphores;
	std::vector<vk::Fence> inFlightFences;
	std::vector<vk::Fence> imagesInFlight;
	int currentFrame = 0;
};
