#version 310 es

in vec3 a_position;
in vec3 a_texcoord;
in vec2 a_dotcoord;

uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_texcoord;
out vec2 v_dotcoord;

void main() {
    gl_Position = u_proj * u_view * vec4(a_position, 1.0);
    v_texcoord = a_texcoord;
    v_dotcoord = a_dotcoord;
}
