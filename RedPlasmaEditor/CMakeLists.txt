find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
# This creates the actual program you will click "Run" on
add_executable(RedPlasmaEditor src/main.cpp)

# This tells the Editor to link to your Engine library
target_link_libraries(RedPlasmaEditor
        PRIVATE
            RedPlasmaEngine
            Vulkan::Vulkan
            glfw
)

# This makes sure the Editor can find the Engine's header files (.hpp)
target_include_directories(RedPlasmaEditor PRIVATE ../RedPlasmaEngine/include)