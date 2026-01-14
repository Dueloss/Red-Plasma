// /*
//  * Red Plasma Engine
//  * Copyright (C) 2026  Kim Johansson
//  *
//  * This program is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU General Public License as published by
//  * the Free Software Foundation...
//  *

//
// Created by Dueloss on 12.01.2026.
//

#include "../include/VulkanGraphicsDevice.h"
#include <iostream>
#include <vector>
struct QueueFamilyIndices {
  int graphicsFamily = -1;
    [[nodiscard]] bool isComplete() const {
        return graphicsFamily >= 0;
    }
};
namespace RedPlasma {
    VulkanGraphicsDevice::VulkanGraphicsDevice() {

    }

    VulkanGraphicsDevice::~VulkanGraphicsDevice() {

    }

    int VulkanGraphicsDevice::Initialize() {

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Red Plasma Engine";
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_EnableExtension.size());
        createInfo.ppEnabledExtensionNames = m_EnableExtension.data();

        if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
            return -1;
        }

        uint32_t devicesCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &devicesCount, nullptr);

        if (devicesCount == 0) {
            std::cout << "No Vulkan instance found!" << std::endl;
            return -2;
        }

        std::vector<VkPhysicalDevice> devices(devicesCount);
        vkEnumeratePhysicalDevices(m_Instance, &devicesCount, devices.data());

        m_PhysicalDevice = devices[0];

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);
        m_DeviceName = deviceProperties.deviceName;

        std::cout << "Vulkan instance name: " << m_DeviceName << std::endl;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

        QueueFamilyIndices indices;

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }
            i++;
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures = {};

        VkDeviceCreateInfo createDeviceInfo {};
        createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createDeviceInfo.pQueueCreateInfos = &queueCreateInfo;
        createDeviceInfo.queueCreateInfoCount = 1;
        createDeviceInfo.pEnabledFeatures = &deviceFeatures;

        if (vkCreateDevice(m_PhysicalDevice, &createDeviceInfo, nullptr, &m_LogicalDevice) != VK_SUCCESS) {
            std::cout << "Vulkan: Failed to create logical device!" << std::endl;
            return -3;
        }

        std::cout << "Vulkan: Logical device created successfully!" << std::endl;

        vkGetDeviceQueue(m_LogicalDevice, indices.graphicsFamily, 0, &m_GraphicsQueue);
        return 0;
    }

    int VulkanGraphicsDevice::Shutdown() {
        std::cout << "Vulkan:: Starting graceful shutdown..." << std::endl;
        if (m_LogicalDevice != VK_NULL_HANDLE) {
            vkDestroyDevice(m_LogicalDevice, nullptr);
            m_LogicalDevice = VK_NULL_HANDLE;
            std::cout << "Vulkan:: Destroying logical device." << std::endl;
        }

        if (m_Surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        if (m_Instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
            std::cout << "Vulkan:: Destroying Instance." << std::endl;
        }
        return 0;
    }

    int VulkanGraphicsDevice::UploadMeshData(const std::vector<Vertex> &vertices) {
        return 0;
    }

    int VulkanGraphicsDevice::DrawFrame() {
        return 0;
    }

    const char * VulkanGraphicsDevice::GetDeviceName() {
        return m_DeviceName;
    }

    int VulkanGraphicsDevice::CreateSurface(IWindowSurface* windowHandle) {
        m_Surface = windowHandle->CreateSurface(m_Instance);

        if (m_Surface == VK_NULL_HANDLE) {
            return -1;
        }
        return -0;
    }

    void VulkanGraphicsDevice::AddExtension(const std::vector<const char*> &extensions) {
        for (auto extension : extensions) {
            m_EnableExtension.push_back(extension);
        }
    }
} // RedPlasma