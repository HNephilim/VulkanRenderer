
#include "Mesh.h"

Mesh::~Mesh()
{

}

Mesh::Mesh()
{

}

Mesh::Mesh(VkPhysicalDevice NewPhysicalDevice, VkDevice NewDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, std::vector<Vertex>* Vertices, std::vector<uint32_t>* Indices)
{
    VertexCount  = Vertices->size();
    IndexCount = Indices->size();

    PhysicalDevice = NewPhysicalDevice;
    Device = NewDevice;

    CreateVertexBuffer(TransferQueue, TransferCommandPool, Vertices);
    CreateIndexBuffer(TransferQueue, TransferCommandPool, Indices);
}
void Mesh::DestroyMeshBuffers()
{
    vkDestroyBuffer(Device, VertexBuffer, nullptr);
    vkFreeMemory(Device, VertexBufferMemory, nullptr);

    vkDestroyBuffer(Device, IndexBuffer, nullptr);
    vkFreeMemory(Device, IndexBufferMemory, nullptr);
}

int Mesh::GetVertexCount()
{
    return VertexCount;
}

VkBuffer Mesh::GetVertexBuffer()
{
    return VertexBuffer;
}

int Mesh::GetIndexCount()
{
    return IndexCount;
}

VkBuffer Mesh::GetIndexBuffer()
{
    return IndexBuffer;
}

void Mesh::CreateVertexBuffer(VkQueue TransferQueue, VkCommandPool TransferCommandPool, std::vector<Vertex>* Vertices)
{
    VkDeviceSize BufferSize = sizeof(Vertex) * Vertices->size();

    // Temporary buffer to "stage" vertex date befor transferring to GPU
    VkBuffer StagingBuffer;
    VkDeviceMemory StagingBufferMemory;

    // CREATE BUFFER AND ALLOCATE MEMORY TO IT
    CreateBuffer(PhysicalDevice, Device, BufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &StagingBuffer, &StagingBufferMemory);

    // MAP MEMORY TO BUFFER
    void* data;                                                                       // 1. Create pointer to a point in normal memory
    vkMapMemory(Device, StagingBufferMemory, 0, BufferSize, 0, &data);    // 2. "Map" the vertex buffer memory to that point
    memcpy(data, Vertices->data(), (size_t)BufferSize);                               // 3. Copy memory from vertices vector to the point
    vkUnmapMemory(Device, StagingBufferMemory);                                        // 4. Unmap the vertex buffer memory

    // Create buffer with VK_BUFFER_USAGE_TRANSFER_DST_BIT to mark as recipient of transfer data (also VERTEX_BUFFER)
    CreateBuffer(PhysicalDevice, Device, BufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,                   // Local to device, only visible to the GPU. Allows it to me optimized for GPU access
                 &VertexBuffer, &VertexBufferMemory);

    // Copy staging buffer to vertex buffer on gpu
    CopyBuffer(Device, TransferQueue, TransferCommandPool, StagingBuffer, VertexBuffer, BufferSize);


    // Clean up tempory staging buffer;
    vkDestroyBuffer(Device, StagingBuffer, nullptr);
    vkFreeMemory(Device, StagingBufferMemory, nullptr);
}

void Mesh::CreateIndexBuffer(VkQueue TransferQueue, VkCommandPool TransferCommandPool, std::vector<uint32_t>* Indices)
{
    //Get size of buffer needed for indices
    VkDeviceSize BufferSize = sizeof(uint32_t) * Indices->size();

    // Temporary buffer to "stage" index data before transferring to GPU
    VkBuffer StagingBuffer;
    VkDeviceMemory StagingBufferMemory;
    CreateBuffer(PhysicalDevice, Device, BufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &StagingBuffer, &StagingBufferMemory);

    // Map Memory to index buffer
    void* data;
    vkMapMemory(Device, StagingBufferMemory, 0, BufferSize, 0, &data);
    memcpy(data, Indices->data(), (size_t)BufferSize);
    vkUnmapMemory(Device, StagingBufferMemory);

    // Create buffer fo INDEX data on GPU access Only area
    CreateBuffer(PhysicalDevice, Device, BufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 &IndexBuffer, &IndexBufferMemory);

    //Copy from staging buffer to GPU access buffer
    CopyBuffer(Device, TransferQueue, TransferCommandPool, StagingBuffer, IndexBuffer, BufferSize);

    //Destroy and realease staging buffer resources
    vkDestroyBuffer(Device, StagingBuffer, nullptr);
    vkFreeMemory(Device, StagingBufferMemory, nullptr);
}





