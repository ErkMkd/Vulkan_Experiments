#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>

#include "helpers.h"

using namespace std;

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;


const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG // "Not debug" C++ standard macro 
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

// VERBOSE: True to display all debug messages. FALSE to display only warnings or errors messages.
#define DEBUG_VERBOSE false

/*
=========================================
	Geometries
=========================================
*/

	struct Vertex {
		glm::vec2 pos;
		glm::vec3 color;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}
		
		static array<VkVertexInputAttributeDescription, 2> getAttributDescriptions() {
			array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // Memes descripteurs que pour les couleurs
			attributeDescriptions[0].offset = offsetof(Vertex, pos);
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			return attributeDescriptions;
		}
	};

	const vector<Vertex> vertices = {
		{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
	};

/*
=========================================
	Main app
=========================================
*/

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainloop();
		cleanup();
	}
	
private:

	GLFWwindow* _window;
	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debugMessenger;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	VkSurfaceKHR _surface;

	VkDevice _logicalDevice;
	
	// Queue
	struct QueueFamilyIndices {
		optional<uint32_t> graphicsFamily;
		optional<uint32_t> presentFamily;
		bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};
	VkQueue _graphicsQueue;
	VkQueue _presentQueue;
	
	// SwapChain
	//Swap chain device extension details
	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		vector<VkSurfaceFormatKHR> formats;
		vector<VkPresentModeKHR> presentModes;
	};
	VkSwapchainKHR _swapChain;
	vector<VkImage> _swapChainImages;
	vector<VkImageView> _swapChainImageViews;
	vector<VkFramebuffer> _swapChainFramebuffers;
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;

	//RenderPass
	VkRenderPass _renderPass;
	VkPipelineLayout _pipelineLayout;
	VkPipeline _graphicsPipeline;

	//Commands
	VkCommandPool _commandPool;
	vector<VkCommandBuffer> _commandBuffers;

	//Asynchrone datas
	const int MAX_FRAMES_IN_FLIGHT = 2;
	vector<VkSemaphore> _imageAvailableSemaphores;
	vector<VkSemaphore> _renderFinishedSemaphores;
	vector<VkFence> _inFlightFences;
	vector<VkFence> _imagesInFlight;
	size_t _currentFrame = 0;
	bool _framebufferResized = false;

	// Geometries
	VkBuffer _vertexBuffer;
	VkDeviceMemory _vertexBufferMemory;
	
	// Needed extensions
	const vector<const char*> _deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	/*
		========================================= Callbacks =========================================
	*/

	void static key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	// Layers validation call back

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback (
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		//Severity:
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_XXX, it's a bit mask: 0x0001, 0x0010, 0x0100, 0x1000 
		// To display all messages : VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		auto level = DEBUG_VERBOSE ? VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT : VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

		//MessageType:
		//VK_DEBUG_UTILS_MESSAGE_TYPE_XXX

		if (messageSeverity >= level) {
			cerr << "Validation layer: " << pCallbackData->pMessage << endl;
		}
		return VK_FALSE;
	}

	/* 
		========================================= Window =========================================
	*/

	void initWindow() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW was originally designed for OpenGL. This line tell GLFW not to create an OpenGL Context.
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
		glfwSetKeyCallback(_window, key_callback);
		glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->_framebufferResized = true;
	}

	/*
		========================================= Debug Extensions =========================================
	*/

	// Check if extensions are available
	vector<const char*> getRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		vector<const char*> extensions(glfwExtensions, glfwExtensionCount + glfwExtensions); // Begin / end

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	// Load extension function vkCreateDebugUtilsMessengerEXT
	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	// Load extension function vkDestroyDebugUtilsMessengerEXT
	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
			cout << "'vkDestroyDebugUtilsMessengerEXT' function loaded and sucessfully called" << endl;
		}
		else
		{
			cout << "ERROR - Unable to load 'vkDestroyDebugUtilsMessengerEXT' function" << endl;
		}
	}

	/*
		========================================= Validation layers ===============================================
		-Disabled in Realease mode
	*/

	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;
			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}
			if (!layerFound) {
				return false;
			}
		}
		cout << "Validation layers OK" << endl;
		return true;
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &_debugMessenger) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to set up debug messenger !");
		}
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = nullptr;
	}

	/*
		========================================= Queue families ===============================================
	*/

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {
				break;
			}

			i++;
		}

		return indices;
	}

	/*
		========================================= Swap chain manager ===============================================
	*/


	void createSwapChain() {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(_physicalDevice);
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		_swapChainImageFormat = surfaceFormat.format;
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		_swapChainExtent = chooseSwapExtent(swapChainSupport.capabilities);
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = _surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = _swapChainExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = findQueueFamilies(_physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		// Create the swapchain
		if (vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapChain) != VK_SUCCESS) {
			throw runtime_error("ERROR - failed to create swap chain !");
		}

		//Get final number of images in swapchain and get images handles
		vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &imageCount, nullptr);
		_swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &imageCount, _swapChainImages.data());
	}

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);
		
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}
		cout << "Warning - Asked surface format unavailable - Used format: " << availableFormats[0].format << " - ColorSpace: " << availableFormats[0].colorSpace << endl;
		return availableFormats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& availablePresentModes) {
		// Check for a prefered present mode
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}
		
		// Fifo is guaranteed to be vailable
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max()) {
			cout << "Warning: HDPI not handled ! Be carefull with resolutions: screen coordinates or pixels ? - " << capabilities.currentExtent.width << endl; // A revoir.
			return capabilities.currentExtent;
		} 
		else {
			int width, height;
			glfwGetFramebufferSize(_window, &width, &height);
			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};
			actualExtent.width = clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
			return actualExtent;
		}
	}

	// Si la fenêtre change de dimension, on recré les framebuffer et les views.
	void recreateSwapChain() {
		int width = 0, height = 0;
		glfwGetFramebufferSize(_window, &width, &height);
		
		// Cas spécial: si la fenêtre est en "minify"(width = height = 0), il ne se passe plus rien:
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(_window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(_logicalDevice);
		cleanupSwapChain();
		createSwapChain();
		createImageViews();
		createFramebuffers();
	}


	/*
		========================================= Physical Devices & extensions ===============================================
	*/

	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		set<string> requiredExtensions(_deviceExtensions.begin(), _deviceExtensions.end());
		for (const auto& extension : availableExtensions) {
			auto search = requiredExtensions.find(extension.extensionName);
			if (search != requiredExtensions.end())
			{
				requiredExtensions.erase(extension.extensionName);
#ifndef NDEBUG
				cout << "Needed device extension found: " << extension.extensionName << endl;
#endif
			}
		}
		return requiredExtensions.empty();
	}


	bool isDeviceSuitable(VkPhysicalDevice device) {
		
		// Check if needed extensions are present
		QueueFamilyIndices indices = findQueueFamilies(device);
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		//Check if swapChain is present
		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}
		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}


	void pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			throw runtime_error("ERROR - Failed to find GPUs with Vulkan support !");
		}
		vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());
		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				_physicalDevice = device;
				break;
			}
		}

		if (_physicalDevice == VK_NULL_HANDLE) {
			throw runtime_error("ERROR - failed to find a suitable GPU !");
		}
	}

	/*
		========================================= Logical Devices =========================================
	*/

	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(_physicalDevice);
		
		vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}
		
		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		
		// Enable device extensions
		createInfo.enabledExtensionCount = static_cast<uint32_t>(_deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = _deviceExtensions.data();
		
		//We initialize device specific validation layers for compatibility, but these layers are up-to-date. Only instance validation layers are used in newer versions of Vulkan.
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_logicalDevice) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to createlogical device !");
		}
		vkGetDeviceQueue(_logicalDevice, indices.graphicsFamily.value(), 0, &_graphicsQueue); // Get graphics queue handle
		vkGetDeviceQueue(_logicalDevice, indices.presentFamily.value(), 0, &_presentQueue); // Get present queue handle

	}


	/* 
		========================================= VK Instance =========================================
		- Vérifie si les Validation Layers sont supportés
		- Vérifie si les extensions GLFW sont présentes
		- Vérifie si les extensions Vulkan dont on a besoin sont présentes
		- Paramétrage des Validation layers

		-Toutes ces informations sont spécifiées dans la structure CreateInfo
		-L'instance est créée
	
	*/



	void createInstance() {

		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw runtime_error("ERROR - Validation layers requested, but not available !");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hi Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		
		// glfw extensions:
		
		auto extensions = getRequiredExtensions(); // Extensions GLFW

		// Vulkan extensions:
		uint32_t vk_extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &vk_extensionCount, nullptr);
		vector<VkExtensionProperties> vk_extensions(vk_extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &vk_extensionCount, vk_extensions.data());
		cout << "Available extensions: \n";
		for (const auto& extension : vk_extensions) {
			cout << '\t' << extension.extensionName << '\n';
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		
		if (enableValidationLayers) {
			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS) {
			throw runtime_error("ERROR - failed to create Vulkan Instance !");
			}
		cout << "Vulkan Instance created" << endl;
	}

	/*
		========================================= Imgage views =========================================
	*/

	void createImageViews() {
		_swapChainImageViews.resize(_swapChainImages.size());
		for (size_t i = 0; i < _swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = _swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = _swapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(_logicalDevice, &createInfo, nullptr, &_swapChainImageViews[i]) != VK_SUCCESS) {
				throw runtime_error("ERROR - Failed to create image views !");
			}
		}

	}

	/*
		========================================= Window surface =========================================
	*/

	void createSurface() {
		if (glfwCreateWindowSurface(_instance, _window, nullptr, &_surface) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to create window surface !");
		}
	}

	/*
		========================================= Graphics pipeline setup =========================================
	*/

	// A simplifier: envoyer directement le nom du fichier ?
	VkShaderModule createShaderModule(const vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to create shader module !");
		}
		return shaderModule;
	}

	void createGraphicsPipeline() {
		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";	// Point d'entrée. Un même shader avec différents points d'entrées peut être utilisé.
		vertShaderStageInfo.pSpecializationInfo = nullptr; // Permet d'initialiser les constantes. Un même shader peut ainsi être paramétré de différentes manières.

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";
		fragShaderStageInfo.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// --- Vertex input

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		
		// --- Input assembly

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// --- Viewport

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) _swapChainExtent.width;
		viewport.height = (float) _swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// --- Scissor

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = _swapChainExtent;


		// --- Dynamic states:

		vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicStateInfo.pDynamicStates = dynamicStates.data();

		// --- Viewport state

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		viewportState.pViewports = &viewport;	// Si spécifiés ici, les viewport et scissor sont statiques
		viewportState.pScissors = &scissor;

		// --- Rasterizer

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		// --- Multisampling

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		// --- Color Blending
			
		/* Blending Pseudo-code:
		* 
			if (blendEnable) {
				finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
				finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
			} else {
				finalColor = newColor;
			}

			finalColor = finalColor & colorWriteMask;
		
		For alpha blending:
			finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
			finalColor.a = newAlpha.a;

		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		
		*/
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		// --- Pipeline layout

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to create pipeline layout !");
		}

		// --- Pipeline Info

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.layout = _pipelineLayout;
		pipelineInfo.renderPass = _renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to create graphics pipeline !");
		}

		// Destroy the shader modules structures - Must be destroyed AFTER pipeline creation
		vkDestroyShaderModule(_logicalDevice, fragShaderModule, nullptr);
		vkDestroyShaderModule(_logicalDevice, vertShaderModule, nullptr);

	}
	/*
		========================================= Render pass =========================================
	*/

	void createRenderPass() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = _swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;


		if (vkCreateRenderPass(_logicalDevice, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to create render pass !");
		}
	}

	/*
		========================================= Framebuffers =========================================
	*/

	void createFramebuffers() {
		_swapChainFramebuffers.resize(_swapChainImageViews.size());
		for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
			VkImageView attachments[] = {_swapChainImageViews[i]};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = _renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = _swapChainExtent.width;
			framebufferInfo.height = _swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(_logicalDevice, &framebufferInfo, nullptr, &_swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw runtime_error("ERROR - Failed to create framebuffer !");
			}
		}
	}

	/*
		========================================= Command pool =========================================
	*/

	void createCommandPool() {
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(_physicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(_logicalDevice, &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to create command pool !");
		}
	}

	/*
		========================================= Memory  =========================================
	*/

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties ){
				return i;
			}
		}
		throw runtime_error("ERROR - Failed to find suitable memory type !");
	}

	/*
		========================================= Vertex buffer =========================================
	*/

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		
		if (vkCreateBuffer(_logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to create buffer !");
		}
		
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(_logicalDevice, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to allocate buffer memory !");
		}

		vkBindBufferMemory(_logicalDevice, buffer, bufferMemory, 0);
	}

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = _commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(_logicalDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(_graphicsQueue);

		vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &commandBuffer);
	}

	void createVertexBuffer() {

		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(_logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(_logicalDevice, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);

		copyBuffer(stagingBuffer, _vertexBuffer, bufferSize);

		vkDestroyBuffer(_logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(_logicalDevice, stagingBufferMemory, nullptr);
	}

	

	/*
		========================================= Command buffer =========================================
	*/

	void createCommandBuffer() {

		_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) _commandBuffers.size();

		if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to allocate command buffers !");
		}
	}

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to begin recording command buffer !");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = _renderPass;
		renderPassInfo.framebuffer = _swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = _swapChainExtent;
		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;
		
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
		
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(_swapChainExtent.width);
			viewport.height = static_cast<float>(_swapChainExtent.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = _swapChainExtent;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			VkBuffer vertexBuffers[] = { _vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to record command buffer !");
		}
	}

	/*
		========================================= Synchronization objects =========================================
	*/

	void createSyncObjects() {

		_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		_imagesInFlight.resize(_swapChainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;	// Activation du Fence à sa construction, car lors du premier appel, aucune image n'a encore été générée.

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(_logicalDevice, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS  ||
				vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS) {
					throw runtime_error("ERROR - Failed to create semaphores and fences !");
				}
		}
	}

	/*
		========================================= Vulkan setup =========================================
	*/

	void initVulkan() {
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createCommandBuffer();
		createSyncObjects();
	}

	/*
		========================================= Clean up =========================================
	*/

	void cleanupSwapChain() {
		for (auto framebuffer : _swapChainFramebuffers) {
			vkDestroyFramebuffer(_logicalDevice, framebuffer, nullptr);
		}
		for (auto imageView : _swapChainImageViews) {
			vkDestroyImageView(_logicalDevice, imageView, nullptr);
		}
		vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr); // Swapchain must be destroyed before logical device
	}

	void cleanup() {
		
		cleanupSwapChain();

		vkDestroyBuffer(_logicalDevice, _vertexBuffer, nullptr);
		vkFreeMemory(_logicalDevice, _vertexBufferMemory, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(_logicalDevice, _renderFinishedSemaphores[i], nullptr);
			vkDestroyFence(_logicalDevice, _inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);

		vkDestroyPipeline(_logicalDevice, _graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(_logicalDevice, _pipelineLayout, nullptr);
		vkDestroyRenderPass(_logicalDevice, _renderPass, nullptr);

		vkDestroyDevice(_logicalDevice, nullptr);
		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
		}
		vkDestroySurfaceKHR(_instance, _surface, nullptr); // Surface must be destroyed before the instance.
		vkDestroyInstance(_instance, nullptr); // Vulkan resources should be destroyed before Instance.
		glfwDestroyWindow(_window);
		glfwTerminate();
	}

	/*
	**************************************************
	Rendering functions
	**************************************************
	*/
	void drawFrame() {

		// ------ Attend que le rendu soit terminé.
		// La queue graphique activera le Fence à la fin du rendu.
		// Pour le premier appel, nous avons activé le Fence à sa construction: VK_FENCE_CREATE_SIGNALED_BIT
		// Le Fence sera désactivé plus bas, pour éviter les ruptures de processus asynchrone.
		vkWaitForFences(_logicalDevice, 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);

		// ------ Acquisition de l'image dispo pour le rendu:
		// Lorsque l'acquisition a été faite, un signal est envoyé via imageAvailableSemaphore
		uint32_t imageIndex; // Image index in the _swapChain
		VkResult result = vkAcquireNextImageKHR(_logicalDevice, _swapChain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

		// ------ Si la taille de la fenêtre a été modifiée, on recréé la swapchain:

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw runtime_error("ERROR - Failed to acquire swap chain image !");
		}


		// Vérifier si une frame précédente est en train d'utiliser cette image
		if (_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(_logicalDevice, 1, &_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}
		// Marque l'image comme étant à nouveau utilisée par cette frame
		_imagesInFlight[imageIndex] = _inFlightFences[_currentFrame];

		vkResetCommandBuffer(_commandBuffers[_currentFrame], 0);

		// ------ Enregistrement des commandes de rendu:
		recordCommandBuffer(_commandBuffers[_currentFrame], imageIndex);


		// ------ Envoie du buffer de commandes à la queue de rendu (graphic queue):
		// Lorsque l'image a bien été acquise, le sémaphore d'attente est activé (imageAvailableSemaphore) et le rendu peut commencer.
		// A la fin du rendu, le sémaphore renderFinishedSemaphore est activé.
		// Le Fence inFlightFence est également activé, un nouveau rendu peu être effectué à partir de ce moment là, pendant que l'image rendue est affichée via la queue de présentation
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { _imageAvailableSemaphores[_currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &_commandBuffers[_currentFrame];

		VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		// Le Fence est désactivé juste avant la soumission de la nouvelle frame.
		vkResetFences(_logicalDevice, 1, &_inFlightFences[_currentFrame]);

		if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to submit draw command buffer !");
		}

		// ------ Envoie de l'image rendue à la queue de présentation:
		// Lorsque le rendu a été fait dans la queue graphique, le renderFinishedSemaphore est activé, et l'image est affichée via la queue de présentation.
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { _swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		result = vkQueuePresentKHR(_presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebufferResized) {
			_framebufferResized = false;
			recreateSwapChain();
		}
		else if ( result != VK_SUCCESS) {
			throw runtime_error("ERROR - Failed to present swap chain image !");
		}
		
		//vkQueueWaitIdle(_presentQueue);

		_currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}


	/* 
	**************************************************
	Main loop 
	**************************************************
	*/

	void mainloop() {
		while (!glfwWindowShouldClose(_window)) {
			glfwPollEvents();
			drawFrame();
		}
		vkDeviceWaitIdle(_logicalDevice); //Attend que les opérations asynchrones en cours se terminent.
	}

};

/*
***************************************************
	Entry point
***************************************************
*/

int main() {
	HelloTriangleApplication app;
#ifdef NDEBUG
	cout << "Release version" << endl;
#else
	cout << "Debug version" << endl;
#endif

	try {
		app.run();
	}
	catch (const std::exception& e) {
		cerr << e.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}