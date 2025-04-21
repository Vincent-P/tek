#version 450

layout(location = 0) out struct {
    vec2 uv;
} g_out;

void main() 
{
    vec2 uv  = vec2(gl_VertexIndex & 2, (gl_VertexIndex << 1) & 2);
    vec3 pos = vec3(uv * 2.0 - 1.0, 1.0);
    
    g_out.uv = uv;
    gl_Position = vec4(pos, 1.0);
}