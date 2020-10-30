#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include "Utilities.h"

class Mesh
{

public:
    Mesh();
    ~Mesh();

    Mesh(VkPhysicalDevice NewPhysicalDevice, VkDevice NewDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, std::vector<Vertex>* Vertices, std::vector<uint32_t>* Indices);
    void DestroyMeshBuffers();

    int GetVertexCount();
    VkBuffer GetVertexBuffer();

    int GetIndexCount();
    VkBuffer GetIndexBuffer();

private:
    int VertexCount;
    VkBuffer VertexBuffer;
    VkDeviceMemory VertexBufferMemory;

    int IndexCount;
    VkBuffer IndexBuffer;
    VkDeviceMemory IndexBufferMemory;

    VkPhysicalDevice PhysicalDevice;
    VkDevice Device;


    void CreateVertexBuffer(VkQueue TransferQueue, VkCommandPool TransferCommandPool, std::vector<Vertex>* Vertices);
    void CreateIndexBuffer(VkQueue TransferQueue, VkCommandPool TransferCommandPool, std::vector<uint32_t>* Indices);

};
