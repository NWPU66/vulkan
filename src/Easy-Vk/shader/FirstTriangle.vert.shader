#version 450
#pragma shader_stage(vertex)

vec2 positions[3] = {
    {    0, -.5f },
    { -.5f,  .5f },
    {  .5f,  .5f }
};

layout(location = 0) out vec3 position;

void main() {
    position = vec3(positions[gl_VertexIndex], 0);
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}