// /*
//  * Red Plasma Engine
//  * Copyright (C) 2026  Kim Johansson
//  *
//  * This program is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU General Public License as published by
//  * the Free Software Foundation...
//  *

//
// Created by Dueloss on 15.01.2026.
//

#ifndef REDPLASMA_VULKANWINDOWSURFACE_H
#define REDPLASMA_VULKANWINDOWSURFACE_H
#include "renderer/IWindowSurface.h"
#include <vulkan/vulkan.h>
namespace RedPlasma {
    class VulkanWindowSurface : public IWindowSurface {
    public:
        VulkanWindowSurface(VkInstance instance, VkSurfaceKHR surface);
        ~VulkanWindowSurface() override;

        [[nodiscard]] void* GetSurfaceHandle() override {return m_Surface;}
        [[nodiscard]] int GetWidth() const override;
        [[nodiscard]] int  GetHeight() const override;

    private:
        VkInstance m_Instance;
        VkSurfaceKHR m_Surface;
        int m_Width = 0;
        int m_Height = 0;
    };
}
#endif //REDPLASMA_VULKANWINDOWSURFACE_H