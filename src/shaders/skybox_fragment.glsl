#version 430 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords);
    //Sample the sky color seen from the given direction in the cubemap using TexCoords. The sky color is entirely determined by the cubemap and is not affected by lighting.
}
