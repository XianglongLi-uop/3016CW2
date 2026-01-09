#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;//The position of the fragment in world space (used for calculating light direction, view direction, etc.).
out vec3 Normal;//Fragment world space normal (for diffuse/specular reflection)
out vec2 TexCoords;//UV coordinates (for sampling 2D textures)
out float Height;// Height value (here using aPos.y), commonly used in terrain snow line, fog effect, color gradient, etc

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    //Transform the vertices from model space to world space
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    //Pass the UV directly to the fragment shader for texture sampling
    Height = aPos.y;
    //Output the y in the model space as the height value
    gl_Position = projection * view * vec4(FragPos, 1.0);
    //Multiply the world space positions in sequence by view and projection to obtain the clip space coordinates, and enter the rasterization stage.
}
