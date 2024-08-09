# hello_triangle
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Stb REQUIRED)
find_package(glog CONFIG REQUIRED)
# file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# file(GLOB HEADER ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
add_executable(hello_triangle 
    # ${SRC}
    # ${HEADER}
    hello_triangle.cpp)
target_include_directories(hello_triangle PRIVATE
    ${Stb_INCLUDE_DIR}
    ${VCPKG_INCLUDE_DIR}
    ${SRC_DIR})
target_link_libraries(hello_triangle PRIVATE 
    assimp::assimp
    glog::glog
    glm::glm
    glfw
    Vulkan::Vulkan)
# add_custom_command(TARGET hello_triangle POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#     "${Open3D_ROOT}/../bin/Open3D.dll"
#     $<TARGET_FILE_DIR:hello_triangle>)

# 移动./shader整个目录到编译目录下
add_custom_command(TARGET hello_triangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/shader"
    $<TARGET_FILE_DIR:hello_triangle>/shader)
add_custom_command(TARGET hello_triangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/texture"
    $<TARGET_FILE_DIR:hello_triangle>/texture)
install(TARGETS hello_triangle 
    RUNTIME DESTINATION ./demo
    LIBRARY DESTINATION ./demo
    ARCHIVE DESTINATION ./demo)
