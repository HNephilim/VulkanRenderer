#include "VulkanRenderer.h"
#include "ValidationLayer.h"


VulkanRenderer::VulkanRenderer()
{
}

VulkanRenderer::~VulkanRenderer()
{
}

int VulkanRenderer::Init(GLFWwindow* NewWindow)
{
	Window = NewWindow;
	try
	{

		CreateInstance();
		CreateSurface();
		GetPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandPool();
        // Create a Mesh
        // VertexData
        std::vector<Vertex> MeshVertices = {
                {.pos{-0.1, -0.4, 0.0},  .col{1.0, 0.0, 0.0}}, // V 0
                {.pos{-0.1, 0.4, 0.0},   .col{0.0, 1.0, 0.0}}, // V 1
                {.pos{-0.9, 0.4, 0.0},  .col{0.0, 0.0, 1.0}}, // V 2
                {.pos{-0.9, -0.4, 0.0}, .col{1.0, 1.0, 0.0}}, // V 3
        };

        std::vector<Vertex> MeshVertices2 = {
                {.pos{0.9, -0.4, 0.0},  .col{1.0, 0.0, 0.0}}, // V 0
                {.pos{0.9, 0.2, 0.0},   .col{0.0, 1.0, 0.0}}, // V 1
                {.pos{0.1, 0.4, 0.0},  .col{0.0, 0.0, 1.0}}, // V 2
                {.pos{0.1, -0.4, 0.0}, .col{1.0, 1.0, 0.0}}, // V 3
        };

        //Index Data
        std::vector<uint32_t> MeshIndices = {
                0, 1, 2,
                2, 3, 0
        };

        Mesh FirstMesh = Mesh(MainDevice.PhysicalDevice, MainDevice.LogicalDevice,
                         GraphicsQueue, GraphicsCommandPool,
                         &MeshVertices, &MeshIndices);
        Mesh SecondMesh = Mesh(MainDevice.PhysicalDevice, MainDevice.LogicalDevice,
            GraphicsQueue, GraphicsCommandPool,
            &MeshVertices2, &MeshIndices);


        MeshList.push_back(FirstMesh);
        MeshList.push_back(SecondMesh);

		CreateCommandBuffer();
		RecordCommands();
		CreateSynchronisation();

	}
	catch (const std::runtime_error& e)
	{
		std::cout << "ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::Draw()
{
    //  -- GET NEXT IMAGE --
    // Wait for givin faceto signal (open) from last draw before continuing
    vkWaitForFences(MainDevice.LogicalDevice, 1, &DrawFences[CurrentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
    // Manually reset (close) fences
    vkResetFences(MainDevice.LogicalDevice, 1, &DrawFences[CurrentFrame]);
    // Get index of next image to be drawn to, and signal semaphore when ready to be drawn to
    uint32_t ImageIndex;
    vkAcquireNextImageKHR(MainDevice.LogicalDevice, Swapchain, std::numeric_limits<uint64_t>::max(), ImageAvailable[CurrentFrame], VK_NULL_HANDLE, &ImageIndex);



    // -- SUBMIT COMMAND BUFFER TO RENDER
    // Queue submission information
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;                          // Number os Semaphores to wait on
    SubmitInfo.pWaitSemaphores = &ImageAvailable[CurrentFrame];               // List of semaphores to wait on
    VkPipelineStageFlags WaitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    SubmitInfo.pWaitDstStageMask = WaitStages;                  // Stages to check semaphores at
    SubmitInfo.commandBufferCount = 1;                          // Number os command buffer to submit
    SubmitInfo.pCommandBuffers = &CommandBuffers[ImageIndex];   // Command buffer to submit
    SubmitInfo.signalSemaphoreCount = 1;                        // Number of semaphores to signal
    SubmitInfo.pSignalSemaphores = &RenderFinished[CurrentFrame];             // Semaphores to signal when command buffer finishes

    // Submit command buffer to queue
    VkResult Result = vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, DrawFences[CurrentFrame]);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to submit Command buffer Graphics Queue");

    // -- PRESENT RENDERED IMAGE TO SCREEN --
    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;                                     // Number of semaphores to wait one
    PresentInfo.pWaitSemaphores = &RenderFinished[CurrentFrame];            // Semaphores to wait on
    PresentInfo.swapchainCount = 1;                                         // Number of swapchains to present to
    PresentInfo.pSwapchains = &Swapchain;                                   // Swapchains to present images to
    PresentInfo.pImageIndices = &ImageIndex;                                // Index of images in swapchains to present

    // Present Image
    Result = vkQueuePresentKHR(GraphicsQueue, &PresentInfo);
    if(Result != VK_SUCCESS) throw  std::runtime_error("Failed to Present Image");

    //Get Next Frame (use % MAX_FRAME_DRAWS to keep value under MAX_FRAME_DRAWS)
    CurrentFrame = (CurrentFrame + 1) % MAX_FRAME_DRAWS;
};

void VulkanRenderer::CleanUp()
{
    // Wait until no actions being run on device before destroying
    vkDeviceWaitIdle(MainDevice.LogicalDevice);

    for (size_t i = 0; i < MeshList.size(); i++)
    {
        MeshList[i].DestroyMeshBuffers();
    }

    for(size_t i=0; i < MAX_FRAME_DRAWS; i++)
    {
        vkDestroySemaphore(MainDevice.LogicalDevice, RenderFinished[i], nullptr);
        vkDestroySemaphore(MainDevice.LogicalDevice, ImageAvailable[i], nullptr);
        vkDestroyFence(MainDevice.LogicalDevice, DrawFences[i], nullptr);
    }

    vkDestroyCommandPool(MainDevice.LogicalDevice, GraphicsCommandPool, nullptr);
    for(auto Framebuffer : SwapchainFramebuffers)
    {
        vkDestroyFramebuffer(MainDevice.LogicalDevice, Framebuffer, nullptr);
    }
    vkDestroyPipeline(MainDevice.LogicalDevice, GraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(MainDevice.LogicalDevice, PipelineLayout, nullptr);
    vkDestroyRenderPass(MainDevice.LogicalDevice, RenderPass, nullptr);
    for(auto Image : SwapchainImages)
    {
        vkDestroyImageView(MainDevice.LogicalDevice, Image.ImageView, nullptr);
    }
	vkDestroySwapchainKHR(MainDevice.LogicalDevice, Swapchain, nullptr);
	vkDestroySurfaceKHR(Instance, Surface, nullptr);
	vkDestroyDevice(MainDevice.LogicalDevice, nullptr);
	vkDestroyInstance(Instance, nullptr);

}

void VulkanRenderer::CreateInstance()
{
	//Setup Validation Layer
	if (bEnableValidationLayers && !CheckValidationLayerSupport()) throw std::runtime_error("Validation Layer Requested, but not available");


	//Information about the application itself
	// Most data here doesn`t affect the program and is for developer convenience
	VkApplicationInfo AppInfo = {};
	AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	AppInfo.pApplicationName = "Vulkan App";				//Custom Name of the app
	AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);	//Custom version of the app
	AppInfo.pEngineName = "No Engine";						//Custon Engine name
	AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);		//Custom engine version
	AppInfo.apiVersion = VK_API_VERSION_1_2;				//The Vulkan Version

	//Building extensions Names and Counts

	//Create list to hold instance extensions
	std::vector<const char*> InstanceExtensions;

	//Set up extensions that our instance will use;
	uint32_t glfwExtensionCount = 0;						//GLFW may require multiple extension
	const char** glfwExtensions;							//Extensions passed as array of cstrings, so need pointer (the array) to pointer (the cstring)

	// Get GLFW Extensions
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//Add GLFW extensions to list of extensions
	for (size_t i = 0; i < glfwExtensionCount; i++)
	{
		InstanceExtensions.push_back(glfwExtensions[i]);
	}
	if (!CheckInstanceExtensionSupport(&InstanceExtensions)) throw std::runtime_error("Some Instance Extensions are not supported");


	//Creation of information for a VKInstace
	VkInstanceCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	CreateInfo.pApplicationInfo = &AppInfo;
	CreateInfo.enabledExtensionCount = static_cast<uint32_t>(InstanceExtensions.size());
	CreateInfo.ppEnabledExtensionNames = InstanceExtensions.data();

	// Setup Validation Layers that Instance will use; Using it only in debugmode
	if (bEnableValidationLayers)
	{
		CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
		CreateInfo.ppEnabledLayerNames = ValidationLayers.data();
	}
	else
	{
		CreateInfo.enabledLayerCount = 0;
		CreateInfo.ppEnabledLayerNames = nullptr;
	}
	

	//Create Instance
	VkResult Result = vkCreateInstance(&CreateInfo, nullptr, &Instance);

	if (Result != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan Instance");
}

void VulkanRenderer::CreateLogicalDevice()
{
	//Get the queue family indices for the chosen PhysicaDevices
	QueueFamilyIndices Indices = GetQueueFamilies(MainDevice.PhysicalDevice);

	//Vector for Queue Creation information and set for family indices
	std::vector<VkDeviceQueueCreateInfo> QueueCreateInfoList;
	std::set<int> QueueFamilyIndices = { Indices.GraphicsFamily, Indices.PresentationFamily };

	//Queues the logical device need to creat and info to do so

	for (int QueueFamilyIndex : QueueFamilyIndices)
	{
		VkDeviceQueueCreateInfo QueueCreateInfo = {};
		QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		QueueCreateInfo.queueFamilyIndex = QueueFamilyIndex;					//Index of the family to create a queue from
		QueueCreateInfo.queueCount = 1;											//Number of queue to create
		float Priority = 1.f;
		QueueCreateInfo.pQueuePriorities = &Priority;							//Vulkans need to know how to handle multiples queue, so decide priority (1 highest, 0 lowest)

		QueueCreateInfoList.push_back(QueueCreateInfo);
	}
	//Physical Device Features the Logical Device will be using
	VkPhysicalDeviceFeatures PhysicalDeviceFeatures = {};					//None will be used by now


	// Information to create logical device (sometimes called "Device")
	VkDeviceCreateInfo DeviceCreateInfo = {};
	DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfoList.size());		//Number of Queue Create Infos
	DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfoList.data();								//List of queue creat infos so device can create required queue
	DeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());		//Number of Ligical Devices extensions
	DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();												//List of Enabled Logical Device Extensions
	DeviceCreateInfo.pEnabledFeatures = &PhysicalDeviceFeatures;									//Physical Device Features the Logical Device will be using

	//Create the logical device for the given physical device
	VkResult Result = vkCreateDevice(MainDevice.PhysicalDevice, &DeviceCreateInfo, nullptr, &MainDevice.LogicalDevice);
	if (Result != VK_SUCCESS) throw std::runtime_error("Failed to create a Logical Device");


	//Queue are created at the same time as the device
	//So we want handle to queue
	//From Given logical device, of given QueueFamily, of given QueueIndex (0 since only one queue), place reference in given VkQueue
	vkGetDeviceQueue(MainDevice.LogicalDevice, Indices.GraphicsFamily, 0, &GraphicsQueue);
	vkGetDeviceQueue(MainDevice.LogicalDevice, Indices.PresentationFamily, 0, &PresentationQueue);
}

void VulkanRenderer::CreateSurface()
{
	//Create Surface (creating a surface creat infostruct, runs the creat surface function, returns result
	VkResult Result = glfwCreateWindowSurface(Instance, Window, nullptr, &Surface);

	if (Result != VK_SUCCESS) throw std::runtime_error("Failed to creat a surface!");
}

void VulkanRenderer::CreateSwapChain()
{
	//GetSwapChain details so we can pick best settings
	SwapChainDetails SwapChainDetails = GetSwapChainDetails(MainDevice.PhysicalDevice);

	//Finding optimal surface values for our swapchain
	VkSurfaceFormatKHR SurfaceFormat = ChooseBestSurfaceFormat(SwapChainDetails.Formats);	
	VkPresentModeKHR PresentationMode = ChooseBestPresentationMode(SwapChainDetails.PresentationModes);	
	VkExtent2D Extent = ChooseSwapExtent(SwapChainDetails.SurfaceCapabilities);

	//How many images are in the swapchain? Get 1 more than the min for triple buffering, but it can't be higher then the max
	//If max is 0, then its infinite.

	uint32_t MinImageCount = SwapChainDetails.SurfaceCapabilities.minImageCount + 1;
	if (SwapChainDetails.SurfaceCapabilities.maxImageCount > 0
		&& MinImageCount > SwapChainDetails.SurfaceCapabilities.maxImageCount)
	{
		MinImageCount = SwapChainDetails.SurfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR SwapChainCreateInfo = {};
	SwapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	SwapChainCreateInfo.surface = Surface;
	SwapChainCreateInfo.minImageCount = MinImageCount;
	SwapChainCreateInfo.imageFormat = SurfaceFormat.format;
	SwapChainCreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
	SwapChainCreateInfo.imageExtent = Extent;
	SwapChainCreateInfo.imageArrayLayers = 1;														// Number of layers for each image in chain
	SwapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;							// What attachmenst images will be used as
	SwapChainCreateInfo.preTransform = SwapChainDetails.SurfaceCapabilities.currentTransform;		// Transform to performe on swap chains images
	SwapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;							// How to hande bending images with external graphics (e.g. other windows)
	SwapChainCreateInfo.presentMode = PresentationMode;
	SwapChainCreateInfo.clipped = VK_TRUE;															//Whether to clip parts of image not in view (e.g. behind other window or out of screen)


	// Get Queue Family indices

	QueueFamilyIndices Indices = GetQueueFamilies(MainDevice.PhysicalDevice);

	//if Grapgics and Presentation families are differente, then swapchain must let imagens be shared between familes
	if (Indices.GraphicsFamily != Indices.PresentationFamily)
	{
		//Creating array of the indices to the queue family
		uint32_t QueueFamilyIndices[] = {
			(uint32_t)Indices.GraphicsFamily,
			(uint32_t)Indices.PresentationFamily
		};

		SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;							// Image share Handling
		SwapChainCreateInfo.queueFamilyIndexCount = 2;												// Number of queues to share images between
		SwapChainCreateInfo.pQueueFamilyIndices = QueueFamilyIndices;								// Array of queue to share between

	}
	else
	{
		SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;							
		SwapChainCreateInfo.queueFamilyIndexCount = 0;												
		SwapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	//If old SwapChain Been Destroyed and this one replaces it, then link opld one to quickly hand over responsabilities (e.g. resize windows)
	SwapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	//Create Swapchain
	VkResult Result = vkCreateSwapchainKHR(MainDevice.LogicalDevice, &SwapChainCreateInfo, nullptr, &Swapchain);
	if (Result != VK_SUCCESS) throw std::runtime_error("Failed to creat a Swapchain");

	//Store for later reference
	SwapchainImageFormat = SurfaceFormat.format;
	SwapchainExtent = Extent;

	uint32_t SwapchainImageCount = 0;
	vkGetSwapchainImagesKHR(MainDevice.LogicalDevice, Swapchain, &SwapchainImageCount, nullptr);
	std::vector<VkImage> Images(SwapchainImageCount);
    vkGetSwapchainImagesKHR(MainDevice.LogicalDevice, Swapchain, &SwapchainImageCount, Images.data());

    for(VkImage Image: Images)
    {
        //Store image handle
        SwapchainImageHandle SwapchainImage = {};
        SwapchainImage.Image = Image;
        SwapchainImage.ImageView = CreateImageView(Image, SwapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        //Add to swapchain image list
        SwapchainImages.push_back(SwapchainImage);
    }
}

void VulkanRenderer::GetPhysicalDevice()
{
	//Enumerate physical devices the VkInstance can access
	uint32_t PhysicalDevicesCount = 0;
	vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount, nullptr);

	//If no Devices available, then none support Vulkan!
	if (PhysicalDevicesCount == 0)throw std::runtime_error("Can't find GPUs that Support Vulkan Instance!");	

	//Create a list to hold all the physical devices
	std::vector<VkPhysicalDevice> PhysicalDevicesList(PhysicalDevicesCount);
	vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount, PhysicalDevicesList.data());

	//Pick a Valid Device for our use
	for (const auto& Device : PhysicalDevicesList)
	{
		if (CheckPhysicalDeviceSuitable(Device))
		{
			MainDevice.PhysicalDevice = Device;
			break;
		}
	}
}

bool VulkanRenderer::CheckInstanceExtensionSupport(std::vector<const char*>* CheckExtensions)
{
	//Need to get number of extensions to create arrat of correct size to hold extensions
	uint32_t ExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);

	//Create a list of VkExtensionProperties using count
	std::vector<VkExtensionProperties> Extensions(ExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.data());

	//Check if given extensions are in list of available extensions

	for (const auto& RequiredExtension : *CheckExtensions)
	{
		bool bHasExtension = false;

		for (const auto& AvailableExtension : Extensions)
		{
			if (strcmp(RequiredExtension, AvailableExtension.extensionName) == 0)
			{
				bHasExtension = true;
				break;
			}
		}
		if (!bHasExtension)
		{
			return false;
		}
	}
	
	return true;
}

bool VulkanRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice PhysicalDevice)
{
	//Get Device extension count
	uint32_t ExtensionCount = 0;
	vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);

	//return false if no extension is false
	if (ExtensionCount == 0) return false;

	//Populate list extension
	std::vector<VkExtensionProperties> Extensions(ExtensionCount);
	vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, Extensions.data());


	//Check for extensions
	for (const auto& DeviceExtension : DeviceExtensions)
	{
		bool bHasExtension = false;
		for (const auto Extension : Extensions)
		{
			if (strcmp(DeviceExtension, Extension.extensionName) == 0)
			{
				bHasExtension = true;
				break;
			}
		}
		if (!bHasExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CheckPhysicalDeviceSuitable(VkPhysicalDevice PhysicalDevice)
{
	/*
	//Information about the device itself (ID, name, type, vendor, etc)
	VkPhysicalDeviceProperties DeviceProperties;
	vkGetPhysicalDeviceProperties(PhysicalDevice, &DeviceProperties);

	//Information about what the device can do (geo shader, tess shader, wide lines, etc)
	VkPhysicalDeviceFeatures DeviceFeatures;
	vkGetPhysicalDeviceFeatures(PhysicalDevice, &DeviceFeatures);
	*/

	QueueFamilyIndices Indices = GetQueueFamilies(PhysicalDevice);

	bool ExtensionsSupported = CheckDeviceExtensionSupport(PhysicalDevice);

	
	bool SwapChainValid = false;

	if (ExtensionsSupported)
	{
		SwapChainDetails SwapChainDetail = GetSwapChainDetails(PhysicalDevice);
		SwapChainValid = !SwapChainDetail.Formats.empty() && !SwapChainDetail.PresentationModes.empty();
	}

	return Indices.IsValid() && ExtensionsSupported && SwapChainValid;
}

QueueFamilyIndices VulkanRenderer::GetQueueFamilies(VkPhysicalDevice PhysicalDevice)
{
	QueueFamilyIndices Indices;

	//Get all Queue Property info for the given device
	uint32_t QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);

	//Create and populate the list of Queue Families
	std::vector<VkQueueFamilyProperties> QueueFamilyList(QueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilyList.data());

	//Go through each queue familly and check if it has at least 1 of the required types of queue
	int i = 0;
	for (const auto& QueueFamily : QueueFamilyList)
	{
		//First check if queue family has at least 1 queue in that family (could have no queue)
		//Queue can be multiple types define thought bitfield. Need to bitwise AND with VK_QUEUE_*_BIT to check if has required type
		if (QueueFamily.queueCount > 0 && QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			Indices.GraphicsFamily = i;				//If queue family is valid, then get index
		}

		//Check if Queue Family supports presentation
		VkBool32 PresentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, i, Surface, &PresentationSupport);
		// Check if queue is presentation type (can be both graphics and presentations)
		if (QueueFamily.queueCount > 0 && PresentationSupport)
		{
			Indices.PresentationFamily = i;
		}

		//Check if QueueFamilyIndices is in a valid state
		if (Indices.IsValid()) break;

		i++;
	}

	return Indices;
}

SwapChainDetails VulkanRenderer::GetSwapChainDetails(VkPhysicalDevice PhysicalDevice)
{
	SwapChainDetails SwapChainDetail;

	// -- CAPABILITIES --
	//Get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SwapChainDetail.SurfaceCapabilities);

	// -- FORMATS --
	uint32_t FormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, nullptr);
	if (FormatCount != 0)
	{
		SwapChainDetail.Formats.resize(FormatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, SwapChainDetail.Formats.data());
	}

	// -- PRESENTATION MODES --
	uint32_t PresentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentationCount, nullptr);
	if (PresentationCount != 0)
	{
		SwapChainDetail.PresentationModes.resize(PresentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentationCount, SwapChainDetail.PresentationModes.data());
	}

	return SwapChainDetail;
}

VkSurfaceFormatKHR VulkanRenderer::ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& Formats)
{
	//Best format is subjective, but ours will be:
	// Format		:		VK_FORMAT_R8G8B8A8_UNORM				(8 bit per color channel + alpha, normalize in a unsigned float (0.0 -> 1.0)
	// ColoRSpace	:		VK_COLOR_SPACE_SRGB_NONLINEAR_KHR		(sRGB with nonlinear gamma function)

	//If all formats is available the system returns undefined, just pick what we wanto
	if (Formats.size() == 1 && Formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM , VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	//If its not undefined, look for what we want(RGBA) or a backup (BGRA)
	for (const auto& format : Formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	//If not has been found, return the first one the list and hope for the best
	return Formats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& PresentationModes)
{

	//Look for mailbox presentation mode;
	for (const auto& PresentationMode : PresentationModes)
	{
		if (PresentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return PresentationMode;
		}
	}

	//If not available, use fifo as vulkan requires as part of the specification;
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& SurfaceCapabilities)
{
	//If current extent is at numeric limits, then extent can vary. Otherwise is the size of the window
	if (SurfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return SurfaceCapabilities.currentExtent;
	}
	else
	{
		//if value can vary, need to set manually

		//Get window size
		int width, height;
		glfwGetFramebufferSize(Window, &width, &height);

		//Creat new extent using window size
		VkExtent2D NewExtent = {};
		NewExtent.width = static_cast<uint32_t>(width);
		NewExtent.height = static_cast<uint32_t>(height);

		//Surface also defines max and min, so make sure within boundaries by clamping value
		NewExtent.width = std::min(SurfaceCapabilities.maxImageExtent.width, NewExtent.width);
		NewExtent.width = std::max(SurfaceCapabilities.minImageExtent.width, NewExtent.width);

		NewExtent.height = std::min(SurfaceCapabilities.maxImageExtent.height, NewExtent.height);
		NewExtent.height = std::max(SurfaceCapabilities.minImageExtent.height, NewExtent.height);


		return NewExtent;
	}
}

VkImageView VulkanRenderer::CreateImageView(VkImage Image, VkFormat Format, VkImageAspectFlags AspectFlags)
{
    VkImageViewCreateInfo ImageViewCreateInfo = {};
    ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ImageViewCreateInfo.image = Image;                                              //Image to create view for
    ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;                            //Type of image(1D, 2D, 3D, CUBE, ETC)
    ImageViewCreateInfo.format = Format;                                            //Format of image date
    ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;               //Allow remapping of rgbacomponent to other rgba value
    ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // Subresouces allow the view to view only a part of an image
    ImageViewCreateInfo.subresourceRange.aspectMask = AspectFlags;                  // Which Aspect of image to view
    ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;                          //Start mipmap level to view from
    ImageViewCreateInfo.subresourceRange.levelCount = 1;                            // Number of mipmap levels to view
    ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;                        // Start array level to view from
    ImageViewCreateInfo.subresourceRange.layerCount = 1;                            // Number of array levels to view

    //Create image view and return it
    VkImageView ImageView;
    VkResult  Result = vkCreateImageView(MainDevice.LogicalDevice, &ImageViewCreateInfo, nullptr, &ImageView);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to create ImageView");

    return ImageView;
}

void VulkanRenderer::CreateGraphicsPipeline()
{
    // Read in SPIV-V code of shaders
    auto VertexShaderCode = ReadFile("E:/VulkanClassesLION/Shaders/vert.spv");
    auto FragmentShaderCode = ReadFile("E:/VulkanClassesLION/Shaders/frag.spv");

    //Build Shader Modules to link to Graphics Pipeline
    VkShaderModule VertexShaderModule = CreateShaderModule(VertexShaderCode);
    VkShaderModule FragmentShaderModule = CreateShaderModule(FragmentShaderCode);

    // -- SHADER STAGE CREATION INFORMATION --
    //Vertex Stage Creation Information
    VkPipelineShaderStageCreateInfo VertexShaderStageCreateInfo = {};
    VertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    VertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;               // Shader stage name
    VertexShaderStageCreateInfo.module = VertexShaderModule;                      // Shader module to be used by stage
    VertexShaderStageCreateInfo.pName = "main";                                   // Entry point into the Shader

    //Fragment Stage Creation Information
    VkPipelineShaderStageCreateInfo FragmentShaderStageCreateInfo = {};
    FragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    FragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;               // Shader stage name
    FragmentShaderStageCreateInfo.module = FragmentShaderModule;                      // Shader module to be used by stage
    FragmentShaderStageCreateInfo.pName = "main";                                   // Entry point into the Shader

    //Put Shader stage creation indo in a array (required by the Pipeline creation)
    VkPipelineShaderStageCreateInfo ShaderStages[] = {VertexShaderStageCreateInfo, FragmentShaderStageCreateInfo};


    // -- VERTEX INPUT --

    // How the data for a single vertex (including info such as position, colour, texture coords, normal, etc) is as a whole
    VkVertexInputBindingDescription VertexInputBindingDescription = {};
    VertexInputBindingDescription.binding = 0;                              // Can bind multiple streams of date, this defines which one
    VertexInputBindingDescription.stride = sizeof(Vertex);                  // Size of a single vertex object
    VertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // How to move between data after each vertex
                                                                            // VK_VERTEX_INPUT_RATE_VERTEX :: Move on to the next vertex
                                                                            // VK_VERTEX_INPUT_RATE_INSTANCE : Move to a vertex for the next instance


    std::array<VkVertexInputAttributeDescription, 2> VertexInputAttributeDescription;

    // Position Attribute
    VertexInputAttributeDescription[0] = {};
    VertexInputAttributeDescription[0].binding = 0;                        // Which binding the data is at (should be the same as above)
    VertexInputAttributeDescription[0].location = 0;                       // Location in shader where data will be read from
    VertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;// Format the data will take (also helps define size of data)
    VertexInputAttributeDescription[0].offset = offsetof(Vertex, pos);     // Where this attribute is defined in the data for a single vertex

    // Color Attribute
    VertexInputAttributeDescription[1] = {};
    VertexInputAttributeDescription[1].binding = 0;                        // Which binding the data is at (should be the same as above)
    VertexInputAttributeDescription[1].location = 1;                       // Location in shader where data will be read from
    VertexInputAttributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT;// Format the data will take (also helps define size of data)
    VertexInputAttributeDescription[1].offset = offsetof(Vertex, col);     // Where this attribute is defined in the data for a single vertex



    VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo= {};
    VertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    VertexInputStateCreateInfo.pVertexBindingDescriptions = &VertexInputBindingDescription;            //List of Vertex Binding Descriptions (data spacing/stride information)
    VertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(VertexInputAttributeDescription.size());
    VertexInputStateCreateInfo.pVertexAttributeDescriptions = VertexInputAttributeDescription.data();          //List of Vertex Attribute Descriptions (data, format and where to bind to or from)


    // -- INPUT ASSEMBLY --
    VkPipelineInputAssemblyStateCreateInfo  InputAssemblyStateCreateInfo = {};
    InputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;            //Primitive type to assembly our vertices
    InputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;                         //Allow overriding of "strip" topology to start new primitives

    // -- VIEWPORT & SCISSOR --
    //Create a viewport info struct
    VkViewport  Viewport = {};
    Viewport.x = 0.0f;                                              // x start coord
    Viewport.y = 0.0f;                                              // y start coord
    Viewport.width = (float)SwapchainExtent.width;                  // width viewport
    Viewport.height = (float)SwapchainExtent.height;                // height viewport
    Viewport.minDepth = 0.0f;                                       // min framebuffer depth
    Viewport.maxDepth = 1.0f;                                       // max framebuffer depth

    //Create a Scissor info struct
    VkRect2D Scissor = {};
    Scissor.offset = {0,0};                                  //Offset to use region from
    Scissor.extent = SwapchainExtent;                               // Extent to describe region to use, starting at offset

    VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {};
    ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportStateCreateInfo.viewportCount = 1;
    ViewportStateCreateInfo.pViewports = &Viewport;
    ViewportStateCreateInfo.scissorCount = 1;
    ViewportStateCreateInfo.pScissors = &Scissor;

    /* // -- DYNAMIC STATE --
    // Dynamic states to enable
    std::vector<VkDynamicState> DynamicStateEnables;
    DynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);       // Dynamic Viewport : Can resize in command buffer with (vkCmdSetViewport(commandbuffer, 0, 1, &NewViewport)
    DynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);        // Dynamic Scissor : Can resize in command buffer with (vkCmdSetScissor(commandbuffer, 0, 1, &NewScissor)

    // Dynamic State creation info
    VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo = {};
    DynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    DynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(DynamicStateEnables.size());
    DynamicStateCreateInfo.pDynamicStates = DynamicStateEnables.data();
    */

    // -- RASTERIZER --
    VkPipelineRasterizationStateCreateInfo  RasterizationStateCreateInfo = {};
    RasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizationStateCreateInfo.depthClampEnable = VK_FALSE;               // Change if fragments beyond near-far planes are clipped(default) ou clamped to plane
    RasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;        // Wheater to discard data and skip rasterizer. Never creates fragments. Useful for pipelines with no output
    RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;        // How to handle filling points betwenn vertices
    RasterizationStateCreateInfo.lineWidth = 1.0f;                          // How thick lines should be when drawn;
    RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;          // Which faze of a tri to cull
    RasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;       // Winding to determine which side is front
    RasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;                // Whether to add depth bias to fragments (Good for stopping "Shadow Acne"

    // -- MULTISAMPLING --
    VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo = {};
    MultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;                          // Enable multisample shading or not
    MultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;            //Number os samples to use per fragment


    //-- BLENDING --
    // Blending decides how to blend a new color being written to a fragment with the old value
    // Blend Attachment State (how Blending is handled)
    VkPipelineColorBlendAttachmentState ColorBlendAttachmentState = {};
    ColorBlendAttachmentState.blendEnable = VK_TRUE;                                    // Enable Blending
    ColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // Colors to enable blending

    // Blending uses equation : (srcColorBlendFactor * newColor) colorBlendOp (dstColorBlendFactor * old color)
    ColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    // Sumarizing: RGB = (NewColorAlpha*NewColor) + ((1-NewColorAlpha) * old color)

    ColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    ColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    // Sumarazing: A = (1 * NewAlpha) + (0 * OldAlpha)

    VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {};
    ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;                 // Alternative to Calculations is to use logical operations
    ColorBlendStateCreateInfo.attachmentCount = 1;
    ColorBlendStateCreateInfo.pAttachments = &ColorBlendAttachmentState;


    // -- PIPELINE LAYOUT (TODO: APPLY FUTURE DESCRIPTOR SET LAYOUTS) --
    VkPipelineLayoutCreateInfo LayoutCreateInfo = {};
    LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayoutCreateInfo.setLayoutCount = 0;
    LayoutCreateInfo.pSetLayouts = nullptr;
    LayoutCreateInfo.pushConstantRangeCount = 0;
    LayoutCreateInfo.pPushConstantRanges = nullptr;

    //Create Pipeline Layout;
    VkResult Result = vkCreatePipelineLayout(MainDevice.LogicalDevice, &LayoutCreateInfo, nullptr, &PipelineLayout);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to create PipelineLayout");

    // -- DEPTH STENCIL TESTING --
    //TODO: SET UP DPTH STENCIL TESTING


    // -- GRAPHICS PIPELINE CREATION --
    VkGraphicsPipelineCreateInfo PipelineCreateInfo = {};
    PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineCreateInfo.stageCount = 2;                                  // Number os shader stager
    PipelineCreateInfo.pStages = ShaderStages;                          // List of shader stages
    PipelineCreateInfo.pVertexInputState = &VertexInputStateCreateInfo;
    PipelineCreateInfo.pInputAssemblyState = &InputAssemblyStateCreateInfo;
    PipelineCreateInfo.pTessellationState = nullptr;
    PipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;
    PipelineCreateInfo.pRasterizationState = &RasterizationStateCreateInfo;
    PipelineCreateInfo.pMultisampleState = &MultisampleStateCreateInfo;
    PipelineCreateInfo.pDepthStencilState = nullptr;
    PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
    PipelineCreateInfo.pDynamicState = nullptr;
    PipelineCreateInfo.layout = PipelineLayout;                         // Pipeline Layout pipeline should use
    PipelineCreateInfo.renderPass = RenderPass;                         // Render pass description the pipeline is compatible with
    PipelineCreateInfo.subpass = 0;                                     // Subpass of render pass to use with pipeline

    // Pipeline derivative: Can create multiple pipeline that derive from one another for optmisation
    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;         //Existing pipeline to derive from..
    PipelineCreateInfo.basePipelineIndex = -1;                      // Or index to base in case of creating multiples

    // Create Grapgics Pipeline
    Result = vkCreateGraphicsPipelines(MainDevice.LogicalDevice, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &GraphicsPipeline);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to create Graphics Pipeline");

    //Destroy Shadel Modules, no longer needed after Pipeline created
    vkDestroyShaderModule(MainDevice.LogicalDevice, FragmentShaderModule, nullptr);
    vkDestroyShaderModule(MainDevice.LogicalDevice, VertexShaderModule, nullptr);

}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& Code)
{
    //Shader Module Creation Info
    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = Code.size();                                          //Size of code
    ShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(Code.data());          //Pointer to code (of uint32_t pointer type)

    VkShaderModule ShaderModule;
    VkResult Result = vkCreateShaderModule(MainDevice.LogicalDevice, &ShaderModuleCreateInfo, nullptr, &ShaderModule);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to Create ShaderModule!");

    return ShaderModule;
}

void VulkanRenderer::CreateRenderPass()
{
    // Color attachment of render pass
    VkAttachmentDescription ColorAttachment = {};
    ColorAttachment.format = SwapchainImageFormat;                          // Format to use for attachment
    ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                        // Number of samples to write for multisamplig
    ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                   // Describes what to do with attachment before rendering
    ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;                 // Describes what to do with attachment after rendering
    ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;        // Describes what to do with stencils before rendering//
    ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;      // Describes what to do with stencils after rendering

    // Framebuffer data will be stored as an image, but images can be given different data layouts
    // to give optimal use for certaion operation
    ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;              // Image data layout before render pass start
    ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;          // Image data layout after render pass (to change to)

    //Attatchment reference uses and attachment index that refers to index in the attachment list passed to RenderPassCreateInfo
    VkAttachmentReference ColorAttachmentReference = {};
    ColorAttachmentReference.attachment = 0;
    ColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Information about a particular subpass the render pass is using
    VkSubpassDescription  SubpassDescription = {};
    SubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Pipeline Type subpass is to be bound to;
    SubpassDescription.colorAttachmentCount = 1;
    SubpassDescription.pColorAttachments = &ColorAttachmentReference;

    // Need to determine when layout transitions occur using subpass dependencies
    std::array<VkSubpassDependency, 2> SubpassDependencies;

    // Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Transition must happen after...
    SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    SubpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    // But must happen before...
    SubpassDependencies[0].dstSubpass = 0;
    SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    SubpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    SubpassDependencies[0].dependencyFlags = 0;


    // Conversion from VK_IMAGE_LAYOUT_PRESENT_SRC_KHR to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // Transition must happen after...
    SubpassDependencies[1].srcSubpass = 0;
    SubpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    SubpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    // But must happen before...
    SubpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    SubpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    SubpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    SubpassDependencies[1].dependencyFlags = 0;

    VkRenderPassCreateInfo RenderPassCreateInfo = {};
    RenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassCreateInfo.attachmentCount = 1;
    RenderPassCreateInfo.pAttachments = &ColorAttachment;
    RenderPassCreateInfo.subpassCount = 1;
    RenderPassCreateInfo.pSubpasses = &SubpassDescription;
    RenderPassCreateInfo.dependencyCount = static_cast<uint32_t>(SubpassDependencies.size());
    RenderPassCreateInfo.pDependencies = SubpassDependencies.data();

    VkResult Result = vkCreateRenderPass(MainDevice.LogicalDevice, &RenderPassCreateInfo, nullptr, &RenderPass);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to create RenderPass");

}

void VulkanRenderer::CreateFramebuffers()
{
    // Resize Framebuffer count to equal swapchain image count
    SwapchainFramebuffers.resize(SwapchainImages.size());

    // Create a framebuffer for each swap chain image
    for(size_t i=0; i < SwapchainFramebuffers.size(); i++)
    {
        std::array<VkImageView, 1> Attachments = {
            SwapchainImages[i].ImageView
        };

        VkFramebufferCreateInfo FramebufferCreateInfo = {};
        FramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferCreateInfo.renderPass = RenderPass;                                      // Render Pass layout the framebuffer will be used with
        FramebufferCreateInfo.attachmentCount = static_cast<uint32_t>(Attachments.size());
        FramebufferCreateInfo.pAttachments = Attachments.data();                            // List of attachments (1:1 with Render Pass
        FramebufferCreateInfo.width = SwapchainExtent.width;                                // Framebuffer width
        FramebufferCreateInfo.height = SwapchainExtent.height;                              // Framebuffer height
        FramebufferCreateInfo.layers = 1;                                                   // Framebuffer layers

        VkResult Result = vkCreateFramebuffer(MainDevice.LogicalDevice, &FramebufferCreateInfo, nullptr, &SwapchainFramebuffers[i]);
        if(Result != VK_SUCCESS) throw std::runtime_error("Failed to create Framebuffer!");
    }
}

void VulkanRenderer::CreateCommandPool()
{
    QueueFamilyIndices QueueFamilyIndex = GetQueueFamilies(MainDevice.PhysicalDevice);

    VkCommandPoolCreateInfo CommandPoolCreateInfo = {};
    CommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolCreateInfo.queueFamilyIndex = QueueFamilyIndex.GraphicsFamily;       // Queue Family type that buffer from this command pool will use

    //Create a Graphics Queue Family Command Pool

    VkResult Result = vkCreateCommandPool(MainDevice.LogicalDevice, &CommandPoolCreateInfo, nullptr, &GraphicsCommandPool);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to create CommandPool");

}

void VulkanRenderer::CreateCommandBuffer()
{
    // Resize CommandBuffer count to have one for each framebuffer
    CommandBuffers.resize(SwapchainFramebuffers.size());

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {};
    CommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandPool = GraphicsCommandPool;
    CommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;              // Primary buffer you submit directly to queue. Cant be called by other buffer.
                                                                                    // Secondaty buffer can't be called directly. Can be called from other buffer via vkCmdExecuteCommands(buffer) when recording commands in primaty
    CommandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(CommandBuffers.size());

    // Allocate command buffers and place handles in array of buffers
    VkResult Result = vkAllocateCommandBuffers(MainDevice.LogicalDevice, &CommandBufferAllocateInfo, CommandBuffers.data());
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to allocate command buffer");
}

void VulkanRenderer::RecordCommands()
{
    // Information about how to begin each command buffer
    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;        // Buffer can be resubmitted when it has already been submitted and is await execution

    // Information about how to begin a render pass (only need for graphical app)
    VkRenderPassBeginInfo RenderPassBeginInfo = {};
    RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassBeginInfo.renderPass = RenderPass;                        // Render Pass to begin
    RenderPassBeginInfo.renderArea.offset = {0, 0};              // Start Point of render pass in pixels
    RenderPassBeginInfo.renderArea.extent = SwapchainExtent;            // Size of region to run render pass on (starting at offset)
    VkClearValue ClearValue[] = {
            {0.6f, 0.65f, 0.4f, 1.0}
    };
    RenderPassBeginInfo.pClearValues = ClearValue;                      // List of clear values (TODO: Depth Attachment Clear Value)
    RenderPassBeginInfo.clearValueCount = 1;                            //



    for(size_t i = 0; i < CommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = SwapchainFramebuffers[i];
        // Start recording commands to command buffer!
        VkResult Result = vkBeginCommandBuffer(CommandBuffers[i], &CommandBufferBeginInfo);
        if(Result != VK_SUCCESS) throw std::runtime_error("Failed to start recording a Command Buffer!");
            // Begin Render pass
            vkCmdBeginRenderPass(CommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                //Bind Pipeline to be used in render pass
                vkCmdBindPipeline(CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);


                for (size_t j = 0; j < MeshList.size(); j++)
                {
                    //Bind Vertex Buffer
                    VkBuffer VertexBuffer[] = { MeshList[j].GetVertexBuffer() };            // Buffers to bind
                    VkDeviceSize  Offsets[] = { 0 };                                      // Offsets into buffers being bound
                    vkCmdBindVertexBuffers(CommandBuffers[i], 0, 1, VertexBuffer, Offsets); // Command to bind vertex buffer before drawing

                    // Bind Mesh index buffer, with 0 offset and using uint32 type
                    vkCmdBindIndexBuffer(CommandBuffers[i], MeshList[j].GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                    //Execute Pipeline
                    vkCmdDrawIndexed(CommandBuffers[i], MeshList[j].GetIndexCount(), 1, 0, 0, 0);
                }
                



            // End Render Pass
            vkCmdEndRenderPass(CommandBuffers[i]);

        //Stop Recording to command buffer
        Result = vkEndCommandBuffer(CommandBuffers[i]);
        if(Result != VK_SUCCESS) throw std::runtime_error("Failed to stop recording a Command Buffer!");

    }
}

void VulkanRenderer::CreateSynchronisation()
{
    ImageAvailable.resize(MAX_FRAME_DRAWS);
    RenderFinished.resize(MAX_FRAME_DRAWS);
    DrawFences.resize(MAX_FRAME_DRAWS);

    // Semaphore Creation Information;
    VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
    SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Fence Creation Information;
    VkFenceCreateInfo FenceCreateInfo = {};
    FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        if(vkCreateSemaphore(MainDevice.LogicalDevice, &SemaphoreCreateInfo, nullptr, &ImageAvailable[i]) != VK_SUCCESS
           || vkCreateSemaphore(MainDevice.LogicalDevice, &SemaphoreCreateInfo, nullptr, &RenderFinished[i]) != VK_SUCCESS
           || vkCreateFence(MainDevice.LogicalDevice, &FenceCreateInfo, nullptr, &DrawFences[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to Create a Semaphore && || fences");
    }


}




