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

#ifndef REDPLASMA_VULKANGRAPHICSDEVICE_H
#define REDPLASMA_VULKANGRAPHICSDEVICE_H
#include "IGraphicsDevice.h"
#include <vulkan/vulkan.h>

namespace RedPlasma {
    class VulkanGraphicsDevice : public IGraphicsDevice {
        public:
        VulkanGraphicsDevice();
        ~VulkanGraphicsDevice() override;

        int Initialize() override;
        int Shutdown() override;
        int UploadMeshData(const std::vector<Vertex>& vertices) override;
        int DrawFrame() override;
        const char* GetDeviceName() override;

        int CreateSurface(IWindowSurface* windowHandle) override;
        void AddExtension(const std::vector<const char*> &extensions) override;
        int InitializeDevice(IWindowSurface* surface);
        int SetupSwapChain(IWindowSurface* surface);
        int CreateImageViews();
        int CreateRenderPass();
        int CreateGraphicsPipeline();
        int CreateFramebuffers();
        int CreateCommandPool();
        int CreateSyncObjects();
        int RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void WaitIdle();

    private:
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_LogicalDevice = VK_NULL_HANDLE;
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        const char* m_DeviceName = "Vulkan Backend";
        VkQueue m_PresentQueue = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        std::vector<const char*> m_EnableExtension;
        int m_graphicsFamilyIndex = 0;
        VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
        std::vector<VkImage> m_SwapChainImages;
        VkFormat m_SwapChainImageFormat;
        VkExtent2D m_SwapChainExtent;
        std::vector<VkImageView> m_SwapChainImageViews;
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_Framebuffers;
        int m_PresentFamilyIndex = -1;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_CommandBuffers;

        VkSemaphore m_ImageAvailableSemaphore;
        VkSemaphore m_RenderFinishedSemaphore;
        VkFence m_InFlightFence = VK_NULL_HANDLE;
    };
} // RedPlasma

#endif //REDPLASMA_VULKANGRAPHICSDEVICE_H