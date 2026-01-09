#version 430 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in float Height;

uniform sampler2D texture_diffuse1;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform bool useTexture;
uniform bool useDynamicLight;
uniform float time;




// The mixed part of terrain biomes, the shader will blend three textures of grass, rock and snow by region, and the fragment shader will blend them according to the weights
uniform bool isTerrain;
uniform sampler2D grassTexture;
uniform sampler2D rockTexture;
uniform sampler2D snowTexture;
void main()
{
    vec3 norm = normalize(Normal);
    
    //  the dynamic light position, The actual position of the light source used in this frame, with useDynamicLight kept on, 
    //the light source moves in a circular path around the original lightPos on the XZ plane with a radius of 5. Eventually, the light will circle around the scene, creating dynamic lighting changes.
    vec3 currentLightPos = lightPos;
    if(useDynamicLight) {
        currentLightPos = lightPos + vec3(sin(time) * 5.0, 0.0, cos(time) * 5.0);
    }
    vec3 lightDir = normalize(currentLightPos - FragPos);
    
    // The ambient light is turned on to simulate "global scattered light", preventing the backlit side from becoming completely black. The ambient light intensity, ambientStrength, is set to 0.3.
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // The diffuse reflection is set as Lambertian lighting, and the cosine of the angle between the normal and the light direction, dot(norm, lightDir), 
    //as well as the product of the diffuse reflection intensity and the light color, diffuse, are defined.
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    // Set the camera direction, incident light direction, and highlight index for mirror reflection.
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result;
    
    if(isTerrain) {
        // The Position Determination Method for Initializing Textures(xPos, zPos):)
        float xPos = TexCoords.x;
        float zPos = TexCoords.y;
        
        // Angle initialization
        float angle = atan(zPos, xPos);
        float angleDeg = angle * 180.0 / 3.14159;
        if (angleDeg < 0.0) angleDeg += 360.0;
        
        // Set the width of the "blending transition" between different terrain areas to 25.
        float transitionWidth = 25.0;
        
        // Calculate the distance to the center of each area.
        // Grass: 270 
        float grassDist = min(abs(angleDeg - 270.0), 360.0 - abs(angleDeg - 270.0));
        // rock:150
        float rockDist = min(abs(angleDeg - 150.0), 360.0 - abs(angleDeg - 150.0));
        // snow:30
        float snowDist;
        if (angleDeg > 180.0) {
            snowDist = min(abs(angleDeg - 390.0), abs(angleDeg - 30.0));
        } else {
            snowDist = abs(angleDeg - 30.0);
        }
        
        // Set the influence range of each mixed texture as: "Base 60° + Transition Width", and then use smoothstep to transform the linear weights into smooth curves (with softer boundaries and no abrupt changes).
        float grassRange = 60.0 + transitionWidth;
        float rockRange = 60.0 + transitionWidth;
        float snowRange = 60.0 + transitionWidth;
        
        float grassWeight = 0.0;
        float rockWeight = 0.0;
        float snowWeight = 0.0;
        
        if (grassDist < grassRange) {
            grassWeight = 1.0 - (grassDist / grassRange);
            grassWeight = smoothstep(0.0, 1.0, grassWeight);
        }
        if (rockDist < rockRange) {
            rockWeight = 1.0 - (rockDist / rockRange);
            rockWeight = smoothstep(0.0, 1.0, rockWeight);
        }
        if (snowDist < snowRange) {
            snowWeight = 1.0 - (snowDist / snowRange);
            snowWeight = smoothstep(0.0, 1.0, snowWeight);
        }
        
        //Normalized weights
        float totalWeight = grassWeight + rockWeight + snowWeight + 0.001;
        grassWeight /= totalWeight;
        rockWeight /= totalWeight;
        snowWeight /= totalWeight;
        
        // Each area uses different UV scaling and offset. The rock texture has the highest repetition density, while the snowfield has the lowest.
        vec2 grassUV = FragPos.xz * 0.08 + vec2(0.0, 0.0);
        vec2 rockUV = FragPos.xz * 0.1 + vec2(17.3, 23.7);
        vec2 snowUV = FragPos.xz * 0.06 + vec2(41.2, 19.5);
        
        // detail layer setting
        vec2 detailUV = FragPos.xz * 0.3;
        
        vec3 grassColor = texture(grassTexture, grassUV).rgb;
        grassColor = mix(grassColor, texture(grassTexture, grassUV * 3.0 + vec2(5.1, 7.3)).rgb, 0.3);
        
        vec3 rockColor = texture(rockTexture, rockUV).rgb;
        rockColor = mix(rockColor, texture(rockTexture, rockUV * 2.5 + vec2(11.2, 13.7)).rgb, 0.35);
        
        vec3 snowColor = texture(snowTexture, snowUV).rgb;
        snowColor = mix(snowColor, texture(snowTexture, snowUV * 2.0 + vec2(3.7, 9.1)).rgb, 0.25);
        
        vec3 terrainColor = grassColor * grassWeight + rockColor * rockWeight + snowColor * snowWeight;
        result = (ambient + diffuse + specular) * terrainColor;
    }
    else if(useTexture) {
        vec3 texColor = texture(texture_diffuse1, TexCoords).rgb;
        result = (ambient + diffuse + specular) * texColor;
    }
    else {
        result = (ambient + diffuse + specular) * objectColor;
    }
    
    FragColor = vec4(result, 1.0);
}
