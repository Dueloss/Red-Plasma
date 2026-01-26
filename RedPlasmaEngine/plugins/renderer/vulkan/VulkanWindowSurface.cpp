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
#include "VulkanWindowSurface.h"

namespace RedPlasma {

    VulkanWindowSurface::VulkanWindowSurface(VkInstance instance, VkSurfaceKHR surface)
        : m_Instance(instance), m_Surface(surface) {}

    VulkanWindowSurface::~VulkanWindowSurface() {
        if (m_Surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        }
    }

    int VulkanWindowSurface::GetWidth() const {
        return m_Width;
    }

    int VulkanWindowSurface::GetHeight() const {
        return m_Height;
    }

} // namespace RedPlasma