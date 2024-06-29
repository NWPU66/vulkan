#pragma once
// c library
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <ciso646>
#include <climits>
#include <cmath>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>

// 可能会用上的C++标准库
#include <chrono>
#include <concepts>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <numbers>
#include <numeric>
#include <span>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <vector>

// GLM
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
/*
在OpenGL中，NDC（标准化设备坐标系）的深度范围为[-1, 1]，而Vulkan中这个范围为[0, 1]，
因此我们必须用宏GLM_FORCE_DEPTH_ZERO_TO_ONE来指定深度范围，
这样才能获得正确的投影矩阵。
*/
// 如果你惯用左手坐标系，在此定义GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// stb_image.h
#include <stb_image.h>

// Vulkan
#ifdef _WIN32                          // 考虑平台是Windows的情况
#    define VK_USE_PLATFORM_WIN32_KHR  // 在包含vulkan.h前定义该宏，会一并包含vulkan_win32.h和windows.h
#    define NOMINMAX  // 定义该宏可避免windows.h中的min和max两个宏与标准库中的函数名冲突
// #    pragma comment(lib, "vulkan-1.lib")  // 链接编译所需的静态存根库
#endif
#include <vulkan/vulkan.h>

// glog
#include <glog/logging.h>
