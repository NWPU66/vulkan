# vk_system_init
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Stb REQUIRED)
find_package(glog CONFIG REQUIRED)
# file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# file(GLOB HEADER ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
add_executable(vk_system_init 
    # ${SRC}
    # ${HEADER}
    vk_system_init.cpp
    )
target_include_directories(vk_system_init PRIVATE
    ${VCPKG_INCLUDE_DIR}
    ${SRC_DIR})
target_link_libraries(vk_system_init PRIVATE 
    glog::glog
    glm::glm
    glfw
    Vulkan::Vulkan)
# add_custom_command(TARGET vk_system_init POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#     "${Open3D_ROOT}/../bin/Open3D.dll"
#     $<TARGET_FILE_DIR:vk_system_init>)

# 移动./shader整个目录到编译目录下
# add_custom_command(TARGET vk_system_init POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#     "${CMAKE_CURRENT_SOURCE_DIR}/shader"
#     $<TARGET_FILE_DIR:vk_system_init>/shader)
install(TARGETS vk_system_init 
    RUNTIME DESTINATION ./demo
    LIBRARY DESTINATION ./demo
    ARCHIVE DESTINATION ./demo)
