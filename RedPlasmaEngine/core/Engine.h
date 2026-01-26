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

#ifndef REDPLASMA_ENGINE_H
#define REDPLASMA_ENGINE_H
namespace RedPlasma {
    class IWindowSurface;
    class IGraphicsDevice;
    struct NativeWindowHandle;

    class Engine {
    public:
        Engine();
        ~Engine();

        int AttachWindow(IWindowSurface* windowHandle);
        void Run() const;
        void Shutdown();
    private:
        bool m_IsRunning;
        IGraphicsDevice* m_GraphicsDevice;

    };
}
#endif //REDPLASMA_ENGINE_H