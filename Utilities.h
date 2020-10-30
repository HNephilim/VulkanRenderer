#pragma once
#include <vector>
#include <fstream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "GLM/glm.hpp"

const int MAX_FRAME_DRAWS = 2;


const std::vector<const char*> DeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//Indices (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices {
	int GraphicsFamily = -1;					//Location of Graphics Queue Family
	int PresentationFamily = -1;				//Location of Presentation Queue Family


	//Function to check if Queue is Valid
	bool IsValid()
	{
		return GraphicsFamily >= 0 && PresentationFamily >= 0;
	}
};

//Holds the Physical device and the Logical Device that  is going to be created;
struct DeviceHandles
{
	VkPhysicalDevice PhysicalDevice;
	VkDevice LogicalDevice;
};

struct SwapChainDetails 
{
	VkSurfaceCapabilitiesKHR SurfaceCapabilities;					//Surface properties, e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> Formats;						//Surface image Formats, e.g. RGBA and size of channel
	std::vector<VkPresentModeKHR> PresentationModes;				//How images should be presented to screen
};

struct SwapchainImageHandle
{
	VkImage Image;
	VkImageView ImageView;
};

static std::vector<char> ReadFile(const std::string& Filename)
{
    //Open stream from given file
    //std::ios""binary tells stream to read file as binary
    //std::ios::ate tells stream to start reading from end of file
    std::ifstream file(Filename, std::ios::binary | std::ios::ate);


    //check if file stream successfully opend
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open a file");
    }

    //Get current read position and use to resize file buffer
    size_t FileSize = (size_t)file.tellg();
    std::vector<char> FileBuffer(FileSize);

    //Move read position (seek to) the start of the file
    file.seekg(0);

    //Read the file data into the buffer (stream "FileSize" in total)
    file.read(FileBuffer.data(), FileSize);

    //Close Stream
    file.close();

    return FileBuffer;
};

struct Vertex
{
    glm::vec3 pos; // Vertex Position (x, y, z)
    glm::vec3 col; // Vertex Color (r, g, b)
};

static uint32_t FindMemoryTypeIndex(VkPhysicalDevice PhysicalDevice, uint32_t AllowedTypes, VkMemoryPropertyFlags Properties)
{
    // Get Properties of physical device memory
    VkPhysicalDeviceMemoryProperties PhysicalDeviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &PhysicalDeviceMemoryProperties);

    for(uint32_t i = 0; i < PhysicalDeviceMemoryProperties.memoryTypeCount; i++)
    {
        if( (AllowedTypes & (1 << i) )                                                       // Index of memory type must match corresponding bit in AllowedTypes
            && ( (PhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties ) ) // Desired property bit flags are parte of memory type's propertie flags
        {
            // This memory type is valid, so return it's index
            return i;
        }
    }

    throw std::runtime_error("Failed to find Memory Type Index");
}

static void CreateBuffer(VkPhysicalDevice PhysicalDevice, VkDevice Device, VkDeviceSize BufferSize, VkBufferUsageFlags BufferUsage, VkMemoryPropertyFlags BufferProperty, VkBuffer* Buffer, VkDeviceMemory* BufferMemory)
{
    //CREATE BUFFER
    // Information to create a buffer (doesn't include assingning memory)
    VkBufferCreateInfo BufferCreateInfo = {};
    BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferCreateInfo.size = BufferSize;  // Size of buffer (size of 1 vertex * number of vertices)
    BufferCreateInfo.usage = BufferUsage; // Multiple types of buffer posible
    BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;   //Similar to Swap Chain Images, Can share vertex Buffer;

    VkResult Result = vkCreateBuffer(Device, &BufferCreateInfo, nullptr, Buffer);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to create Vertex Buffer");


    //GET BUFFER MEMORY REQUIREMENTS
    VkMemoryRequirements MemoryRequirements;
    vkGetBufferMemoryRequirements(Device, *Buffer, &MemoryRequirements);


    //ALLOCATE MEMORY TO BUFFER
    VkMemoryAllocateInfo MemoryAllocateInfo = {};
    MemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = FindMemoryTypeIndex(PhysicalDevice,
                                                             MemoryRequirements.memoryTypeBits,         // Index of memory type on physical decive that has required bit flags
                                                             BufferProperty);                           // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = CPU can interact with memory
                                                                                                        // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = Allows placement of data straight int buffer after mapping (otherwise would have to specify manually)

    // Allocate memory to VkDeviceMemory
    Result = vkAllocateMemory(Device, &MemoryAllocateInfo, nullptr, BufferMemory);
    if(Result != VK_SUCCESS) throw std::runtime_error("Failed to Allocate Vertex Buffer");

    // Allocate memory to given vertex buffer
    vkBindBufferMemory(Device, *Buffer, *BufferMemory, 0);
}

static void CopyBuffer(VkDevice Device, VkQueue TransferQueue, VkCommandPool TransferCommandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize BufferSize)
{
    // ALLOCATE COMMAND BUFFER
    //Command Buffer to hold transfer commands
    VkCommandBuffer TransferCommandBuffer;

    // Command buffer details
    VkCommandBufferAllocateInfo AllocateInfo = {};
    AllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    AllocateInfo.commandPool = TransferCommandPool;
    AllocateInfo.commandBufferCount = 1;

    //Allocate command buffer from pool;
    vkAllocateCommandBuffers(Device, &AllocateInfo, &TransferCommandBuffer);


    // WRITE COMMAND BUFFER
    // Information to begin the command buffer record
    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; //We're only using the command buffer once, so set up for one time only

    // Begin recording transfer commands
    vkBeginCommandBuffer(TransferCommandBuffer, &CommandBufferBeginInfo);

    //Region of data to copy from and to
    VkBufferCopy BufferCopyRegion = {};
    BufferCopyRegion.srcOffset = 0;             //Where to start copying from
    BufferCopyRegion.dstOffset = 0;             //Where to start copying to
    BufferCopyRegion.size = BufferSize;

    //Cmd to copy src buffer to dst buffer
    vkCmdCopyBuffer(TransferCommandBuffer, srcBuffer, dstBuffer, 1, &BufferCopyRegion);

    // End commands
    vkEndCommandBuffer(TransferCommandBuffer);


    //SUBMIT COMMAND BUFFER
    //Queue submission information
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &TransferCommandBuffer;

    // Submit transfer command to transfer queue and wait until it finishes
    vkQueueSubmit(TransferQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(TransferQueue);



    // FREE TEMPO COMMANDBUFFER BACK TO POOL
    vkFreeCommandBuffers(Device, TransferCommandPool, 1, &TransferCommandBuffer);

}