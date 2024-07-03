# easy_vk
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Stb REQUIRED)
find_package(glog CONFIG REQUIRED)
file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB HEADER ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
add_executable(easy_vk 
    ${SRC}
    ${HEADER}
    )
target_include_directories(easy_vk PRIVATE
    ${VCPKG_INCLUDE_DIR}
    ${SRC_DIR})
target_link_libraries(easy_vk PRIVATE 
    assimp::assimp
    glog::glog
    glm::glm
    glfw
    Vulkan::Vulkan)
# add_custom_command(TARGET easy_vk POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#     "${Open3D_ROOT}/../bin/Open3D.dll"
#     $<TARGET_FILE_DIR:easy_vk>)

# 移动./shader整个目录到编译目录下
add_custom_command(TARGET easy_vk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/shader"
    $<TARGET_FILE_DIR:easy_vk>/shader)
install(TARGETS easy_vk 
    RUNTIME DESTINATION ./demo
    LIBRARY DESTINATION ./demo
    ARCHIVE DESTINATION ./demo)
