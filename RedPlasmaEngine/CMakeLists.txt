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
        include/VulkanWindowSurface.h
        src/VulkanWindowSurface.cpp
)

# Tell the engine where to find its own header files
target_include_directories(RedPlasmaEngine PUBLIC include)
# Find Vulkan (This is what you'll need for the triangle!)
find_package(Vulkan REQUIRED COMPONENTS glslangValidator)
set(SHADER_SOURCES
        "shaders/shader.vert"
        "shaders/shader.frag"
)

# 3. Create a command to compile each shader
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER} EXT)
    # Remove the dot from extension (e.g., .vert -> vert)
    string(SUBSTRING ${SHADER_EXT} 1 -1 SHADER_TYPE)

    set(SPV_OUTPUT "${CMAKE_BINARY_DIR}/RedPlasmaEditor/shaders/${SHADER_TYPE}.spv")

    add_custom_command(
            OUTPUT ${SPV_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/RedPlasmaEditor/shaders"
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
