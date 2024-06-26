set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# add sub directories
subdirs(
    ${CMAKE_CURRENT_SOURCE_DIR}/VulkanCodeGuide
    ${CMAKE_CURRENT_SOURCE_DIR}/Easy-Vk
    ${CMAKE_CURRENT_SOURCE_DIR}/util
)

# helloworld programe
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
add_executable(hello_vulkan hello_vulkan.cpp)
target_include_directories(hello_vulkan PRIVATE
    ${VCPKG_INCLUDE_DIR}
    ${SRC_DIR})
target_link_libraries(hello_vulkan PRIVATE 
    imgui::imgui
    glm::glm)
install(TARGETS hello_vulkan RUNTIME DESTINATION ./)