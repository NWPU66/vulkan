#version 460
#pragma shader_stage(fragment)

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec3 position;

void main() {
    o_Color = vec4(position*2+1, 1);
}

/* 插值修饰符
可以对以下阶段的输入使用插值修饰符：
1.细分控制着色器（从顶点着色器到细分控制着色器本就不会插值，写插值修饰符为了跟顶点
着色器中的修饰符匹配）
2.几何着色器（同样，写插值修饰符为了跟先前着色器中的修饰符匹配）
3.片段着色器

着色器的代码可能适用于某个阶段的着色器可有可无的情况，比如你可以构造出一组着色器，
既能走“顶点→几何→片段”的流程，也能走“顶点→片段”的流程，如果片段着色器中需要
特定插值修饰符的输入，那么顶点着色器中就得使用相应插值修饰符的输出，而为了匹配，
尽管“顶点→几何”不需要插值，这里头几何着色器也得有同样插值修饰符的输入。

插值修饰符有以下三种：
1.smooth是默认的插值方式，它进行双曲插值，会根据gl_Position的w分量考虑透视的影响
（经三维相机效果的投影矩阵变换后生成的顶点，透视的影响会反映在其w值上）。
2.flat不进行插值，对某一片段调用片段着色器时，flat的输入取得先前阶段中，该片段对应
图元的激发顶点（provoking vertex）的相应值。
3.noperspective进行简单粗暴的线性插值，不考虑透视的影响。
如果输出的gl_Position.w总是为1（通常为渲染平面图形时），smooth和noperspective的效果没有差别。
关于激发顶点，请自行参阅Vulkan官方标准中的图示。
*/

/* Push Constant的声明方式
Push constant是在着色器中使用可变更（由CPU侧）常量的两种方式之一。
Push constant适用于少量数据，Vulkan的实现通常会确保你能在push constant块中使用128个字节。

layout(内存布局修饰符, push_constant) uniform 块名称 {
    成员声明
} 实例名称;

若省略内存布局修饰符,，则默认为std430，详见后文块成员的内存布局。
可以没有实例名称，如此一来能直接用成员名称访问块成员。否则用实例名称.成员名称来访问块成员。

块当中的成员可以前缀layout(offset = 距离整块数据起始位置的字节数)修饰，其具体用例：
//顶点着色器中
layout(push_constant) uniform pushConstants {
    mat4 proj;
    vec2 view;
    vec2 scale;
};
//片段着色器中
layout(push_constant) uniform pushConstants {
    layout(offset = 80)
    vec4 color;
};
以上代码摘自一组很简单的用于2D渲染的着色器，其中，顶点着色器中需要proj矩阵、view和scale
两个矢量，而片段着色器只需要color，它们加在一起一共96个字节，可以全部放进push constant中，
片段着色器中只需要声明color，但它和proj、view和scale在同一整块数据中，若不想在片段着色器
中声明proj、view和scale，则必须写明color的offset。
*/

/* Uniform缓冲区的声明方式
Uniform缓冲区（uniform buffer）是在着色器中使用可变更（由CPU侧）常量的两种方式之一，
类似于HLSL中的常量缓冲区。
相比push constant，uniform缓冲区适用于大量数据。

layout(set = 描述符集索引, binding = 绑定索引) uniform 块名称 {
    成员声明
} 实例名称;

若省略set = 描述符集索引,，则默认为0号描述符集。
实例可以为数组，对应的描述符亦构成数组（创建相应描述符布局时
VkDescriptorSetLayoutBinding::descriptorCount大于1）。
同push constant一样，可以没有实例名称，如此一来能直接用成员名称访问块成员。
同push constant一样，块当中的成员可以前缀layout(offset = 距离缓冲区起始位置的字节数)修饰。
*/

/* 其他Uniform对象的声明
注意到上文中声明push constant和uniform缓冲区时皆用到了uniform这个关键字。
Uniform对象指的是着色器中的运行期常量（所谓运行期，指其并非编译期或装配管线时指定），
只读不写，且在单次绘制命令的调用中不会改变其数据。

以下几种uniform对象能以类似的方式声明：

各类贴图：以texture、itexture、utexture开头的一系列类型，如texture2D。无前缀、
i、u前缀分别对应浮点、有符号整形和无符号整形（所涉及注意事项见Ch7-7 使用贴图）。

采样器：有sampler和samplerShadow两种。

带采样器的贴图：以sampler、isampler、usampler开头的一系列类型，如sampler2D和
sampler2DShadow。带Shadow后缀的无i或u前缀版本。

Uniform纹素缓冲区：有textureBuffer、itextureBuffer、utextureBuffer三种（写成OpenGL
中定义的samplerBuffer、isamplerBuffer、usamplerBuffer也没差，反正都跟采样器没关系）。

layout(set = 描述符集索引, binding = 绑定索引) uniform 类型 实例名称;
*/

/* 输入附件的的声明
最后还剩一种uniform对象：子通道输入（subpass input），即输入附件
（input attachment）在GLSL中的概念，有subpssInput、subpssInputMS、isubpssInput、
isubpssInputMS、usubpssInput、usubpssInputMS六种类型，MS后缀说明是多重采样附件。

layout(set = 描述符集索引, binding = 绑定索引, input_attachment_index = 输入附件索引) uniform 类型 实例名称;

输入附件索引对应VkSubpassDescription::pInputAttachments所指代的相应输入附件。
实例可以为数组，对应的描述符构成数组。啊？你问这要怎么构成数组？以下式为例：
layout(binding = 0, input_attachment_index = 基础索引) uniform subpassInput u_GBuffers[3];
通过表达式实例名称[N]进行访问时，访问到VkSubpassDescription::pInputAttachments[基础索引 + N]指代的输入附件。
这是Vulkan的GLSL方言中规定的。在将输入附件对应的image view写入描述符时应当注意顺序。
*/

/* 块成员的内存布局
Vulkan的GLSL着色器中，对于块，允许两种内存布局：std140和std430。不同的内存布局遵循不同的对齐规则。
Uniform缓冲区的内存布局只能为std140。Storage缓冲区和push constant的默认内存布局为std430，可以指定为std140。

std430的对齐遵循以下规则：
1.大小为N的标量，对齐为N。
2.二维矢量，每个分量大小为N，则对齐为2N。
3.三维及四维矢量，每个分量大小为N，则对齐为4N（注意，没有12字节对齐）。
4.C列R行的列主矩阵的对齐，相当于R维矢量的对齐。
5.结构体的对齐，取其成员大小中最大的对齐。
6.数组的对齐，取其单个元素大小的对齐。

std140的对齐比std430更严格：
1.结构体的对齐，取其成员大小中最大的对齐并凑整到16的倍数。
2.数组的对齐，取其单个元素大小的对齐并凑整到16的倍数，
且数组的每个元素之间的步长等于数组的对齐（即步长也凑整到16的倍数，
这会导致由16个int构成的数组的大小是256字节而不是64字节）。

在C++中，可以通过前缀alignas(...)来指定对齐：
struct {
    alignas(16) mat4 proj;         //offset为0
    alignas( 8) vec2 view;         //offset为64
    alignas( 8) vec2 scale;        //offset为72
    alignas( 4) float width;       //offset为80
    alignas( 4) float cornerRadius;//offset为84
    alignas(16) vec4 color0;       //offset为96
    alignas(16) vec4 color1;       //offset为112
} constant;

GLM中提供了的aligned type来帮你在一定程度上省事：
注意GLM提供的aligned type也存在问题：
//GLSL代码，块大小为16
layout(push_constant) uniform pushConstants {
    vec3 color;
    float alpha;    // 这里的alpha是4对齐的，起始地址12满足4对齐，所以会填充在12-16的内存空间上
};
//C++代码，结构体大小为32
struct {
    glm::aligned_vec3 color;
    float alpha;
} constant;
 因为glm::aligned_vec3的大小和对齐都是16（理想的情况是，大小为12，但对齐为16），所以C++代码中alpha没有填入12~16字节间的空位上。
我建议始终手动alignas(...)而非使用GLM的aligned type。
*/

/* 可特化常量的声明方式和使用
layout(constant_id = ID编号) const 类型 常量名称;

需要注意的是，若在uniform块中声明数组时，应当只有一个数组的大小被可特化常量指定，
且该数组应该位于块的最后：

//声明一个可以被特化的常量
layout(constant_id = 0) const uint maxLightCount = 32;
//Case 1: 这么做可以
layout(binding = 0) uniform descriptorConstants {
    vec3 cameraPosition;
    int lightCount;
    light lights[maxLightCount];//light是自定义结构体类型，定义略
};
//Case 2: 不要这么做
layout(binding = 0) uniform descriptorConstants {
    light lights[maxLightCount];
    vec3 cameraPosition;
    int lightCount;
};

各个块成员的offset是被静态计算的，也就是说，Case 2这种情况，cameraPosition
在缓冲区中的位置是根据maxLightCount的默认值进行计算的。假设可以在创建管线
时改变maxLightCount（所谓假设，是因为这种情况下创建管线时Vulkan的验证层会报错），
cameraPosition也总是在32个light的大小之后的位置。
*/

