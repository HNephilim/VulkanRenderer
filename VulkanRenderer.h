#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


#include "Utilities.h"

#include <stdexcept>
#include <vector>
#include <iostream>
#include <set>
#include <algorithm>
#include <array>

#include "Mesh.h"
class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();


	int Init(GLFWwindow* NewWindow);
	void Draw();
	void CleanUp();


private:

	GLFWwindow* Window;
	int CurrentFrame = 0;


	// Scene Objects
	std::vector<Mesh> MeshList;

	//Vulkan Components
	/// - Main
	VkInstance Instance;
	DeviceHandles MainDevice;	
	VkSurfaceKHR Surface;
	VkSwapchainKHR Swapchain;
	VkQueue GraphicsQueue;
	VkQueue PresentationQueue;
	std::vector<SwapchainImageHandle> SwapchainImages;
	std::vector<VkFramebuffer> SwapchainFramebuffers;
    std::vector<VkCommandBuffer> CommandBuffers;
    void CreateSynchronisation();

	/// - Utility
	VkFormat SwapchainImageFormat;
	VkExtent2D SwapchainExtent;

	/// - Pipeline
	VkPipeline GraphicsPipeline;
	VkPipelineLayout PipelineLayout;
	VkRenderPass RenderPass;

	/// - Pools
	VkCommandPool GraphicsCommandPool;

	/// - Synchronisation
	std::vector<VkSemaphore> ImageAvailable;
    std::vector<VkSemaphore> RenderFinished;
    std::vector<VkFence> DrawFences;


	//Vulkan Functions
	/// - Create Functions
	void CreateInstance();
	void CreateLogicalDevice();
	void CreateSurface();
	void CreateSwapChain();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateCommandBuffer();

	/// - Record Functions
	void RecordCommands();

	/// - Get Functions
	void GetPhysicalDevice();

	/// - Support Functions
	//// -- Checker Functions
	bool CheckInstanceExtensionSupport(std::vector<const char*>* CheckExtensions);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice PhysicalDevice);
	bool CheckPhysicalDeviceSuitable(VkPhysicalDevice PhysicalDevice);


	//// -- Getter Functions
	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice PhysicalDevice);
	SwapChainDetails GetSwapChainDetails(VkPhysicalDevice PhysicalDevice);

	//// -- Choose Functions
	VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& Formats);
	VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& PresentationModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& SurfaceCapabilities);

	//// -- Create Functions
	VkImageView CreateImageView(VkImage Image, VkFormat Format, VkImageAspectFlags AspectFlags);
	VkShaderModule CreateShaderModule (const std::vector<char>& Code);

};

