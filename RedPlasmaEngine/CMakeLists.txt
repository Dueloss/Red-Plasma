# Create the Engine as a Shared Library (.so file)
add_library(RedPlasmaEngine SHARED
        src/Engine.cpp
        src/VulkanGraphicsDevice.cpp
        # Headers
        include/Engine.h
        include/IGraphicsDevice.h
        include/IWindowSurface.h
        include/VulkanGraphicsDevice.h
        include/WaylandSurface.h
)

# Tell the engine where to find its own header files
target_include_directories(RedPlasmaEngine PUBLIC include)
# Find Vulkan (This is what you'll need for the triangle!)
find_package(Vulkan REQUIRED)
target_link_libraries(RedPlasmaEngine
        PRIVATE
            Vulkan::Vulkan
            ${WAYLAND_CLIENT_LIBRARIES}
)
