# Create the Engine as a Shared Library (.so file)
add_library(RedPlasmaEngine SHARED
        core/Engine.cpp
        plugins/renderer/vulkan/VulkanGraphicsDevice.cpp
        # Headers
        core/Engine.h
        core/renderer/IGraphicsDevice.h
        core/renderer/IWindowSurface.h
        plugins/renderer/vulkan/VulkanGraphicsDevice.h
        plugins/renderer/vulkan/platform/linux/wayland/WaylandSurface.h
        plugins/renderer/vulkan/VulkanWindowSurface.h
        plugins/renderer/vulkan/VulkanWindowSurface.cpp
        core/RP_Result.h
)

# Tell the engine where to find its own header files
target_include_directories(
        RedPlasmaEngine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}        # Root: Allows #include "core/..."
        ${CMAKE_CURRENT_SOURCE_DIR}/core     # Core: Allows #include "renderer/..."
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/renderer/vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/renderer/vulkan# Local: Find Vulkan files
)


# Find Vulkan (This is what you'll need for the triangle!)
find_package(Vulkan REQUIRED COMPONENTS glslangValidator)
set(SHADER_SOURCES
        "plugins/renderer/vulkan/shaders/shader.vert"
        "plugins/renderer/vulkan/shaders/shader.frag"
)

# 2. Process each shader
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER} EXT)
    string(SUBSTRING ${SHADER_EXT} 1 -1 SHADER_TYPE)

    # Output path in build directory
    set(SPV_OUTPUT "${CMAKE_BINARY_DIR}/RedPlasmaEditor/shaders/${SHADER_TYPE}.spv")

    add_custom_command(
            OUTPUT ${SPV_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/RedPlasmaEditor/shaders"
            # FIX: We use "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}" only once.
            COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}" -o ${SPV_OUTPUT}
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}"
            COMMENT "Compiling shader ${SHADER} to ${SPV_OUTPUT}"
    )
    list(APPEND SPV_BINARY_FILES ${SPV_OUTPUT})
endforeach()

# 4. Create a target so CMake knows to run these commands
add_custom_target(Shaders ALL DEPENDS ${SPV_BINARY_FILES})

# 5. Make the engine depend on the shaders
add_dependencies(RedPlasmaEngine Shaders)

target_link_libraries(RedPlasmaEngine
        PRIVATE
            Vulkan::Vulkan
            ${WAYLAND_CLIENT_LIBRARIES}
)
