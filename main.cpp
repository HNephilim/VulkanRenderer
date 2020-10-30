#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


#include <iostream>
#include <stdexcept>
#include <vector>


#include "VulkanRenderer.h"

GLFWwindow * window;
VulkanRenderer VulkanRender;

void InitWindow(std::string wName = "Test Window", const int width = 800, const int height = 600)
{
	//Inicialize GLFW
	glfwInit();

	//Set GLFW to NOT work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int main()
{

	//Create Window
	InitWindow();

	//Create Vulkan Rederer Instance
	if (VulkanRender.Init(window) == EXIT_FAILURE) return EXIT_FAILURE;

	//Loop until Close

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		VulkanRender.Draw();
	}
	


	//Clean and Destroy
	VulkanRender.CleanUp();
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}