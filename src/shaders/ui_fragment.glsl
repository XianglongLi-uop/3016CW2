#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D signatureTexture;
uniform bool hasTexture;
uniform vec3 textColor;
uniform int displayScore;
uniform bool isScoreDisplay;

// Simple numeric segment code display (7-segment style)
float segment(vec2 uv, vec2 p1, vec2 p2, float w) {
    vec2 d = p2 - p1;
    float l = length(d);
    d /= l;
    vec2 q = uv - p1;
    float t = clamp(dot(q, d), 0.0, l);
    return smoothstep(w, w * 0.5, length(q - d * t));
}

float digit(vec2 uv, int n) {
    float s = 0.0;
    float w = 0.08;
    // Using multiple segments of lines to piece together the number (0-9) in the shader's quadrilateral can achieve the scoring when the player touches the yellow ball for points.
    if(n == 0) { s += segment(uv, vec2(0.2,0.1), vec2(0.8,0.1), w); s += segment(uv, vec2(0.2,0.9), vec2(0.8,0.9), w); s += segment(uv, vec2(0.2,0.1), vec2(0.2,0.9), w); s += segment(uv, vec2(0.8,0.1), vec2(0.8,0.9), w); }
    if(n == 1) { s += segment(uv, vec2(0.5,0.1), vec2(0.5,0.9), w); }
    if(n == 2) { s += segment(uv, vec2(0.2,0.9), vec2(0.8,0.9), w); s += segment(uv, vec2(0.8,0.5), vec2(0.8,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.8,0.5), w); s += segment(uv, vec2(0.2,0.1), vec2(0.2,0.5), w); s += segment(uv, vec2(0.2,0.1), vec2(0.8,0.1), w); }
    if(n == 3) { s += segment(uv, vec2(0.2,0.9), vec2(0.8,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.8,0.5), w); s += segment(uv, vec2(0.2,0.1), vec2(0.8,0.1), w); s += segment(uv, vec2(0.8,0.1), vec2(0.8,0.9), w); }
    if(n == 4) { s += segment(uv, vec2(0.2,0.5), vec2(0.2,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.8,0.5), w); s += segment(uv, vec2(0.8,0.1), vec2(0.8,0.9), w); }
    if(n == 5) { s += segment(uv, vec2(0.2,0.9), vec2(0.8,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.2,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.8,0.5), w); s += segment(uv, vec2(0.8,0.1), vec2(0.8,0.5), w); s += segment(uv, vec2(0.2,0.1), vec2(0.8,0.1), w); }
    if(n == 6) { s += segment(uv, vec2(0.2,0.9), vec2(0.8,0.9), w); s += segment(uv, vec2(0.2,0.1), vec2(0.2,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.8,0.5), w); s += segment(uv, vec2(0.8,0.1), vec2(0.8,0.5), w); s += segment(uv, vec2(0.2,0.1), vec2(0.8,0.1), w); }
    if(n == 7) { s += segment(uv, vec2(0.2,0.9), vec2(0.8,0.9), w); s += segment(uv, vec2(0.8,0.1), vec2(0.8,0.9), w); }
    if(n == 8) { s += segment(uv, vec2(0.2,0.1), vec2(0.8,0.1), w); s += segment(uv, vec2(0.2,0.9), vec2(0.8,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.8,0.5), w); s += segment(uv, vec2(0.2,0.1), vec2(0.2,0.9), w); s += segment(uv, vec2(0.8,0.1), vec2(0.8,0.9), w); }
    if(n == 9) { s += segment(uv, vec2(0.2,0.9), vec2(0.8,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.2,0.9), w); s += segment(uv, vec2(0.2,0.5), vec2(0.8,0.5), w); s += segment(uv, vec2(0.8,0.1), vec2(0.8,0.9), w); s += segment(uv, vec2(0.2,0.1), vec2(0.8,0.1), w); }
    return clamp(s, 0.0, 1.0);
}

// THE personal signature "Dragon.L" rendering functions
float letterD(vec2 uv) {
    float s = 0.0;
    float w = 0.08;
    s += segment(uv, vec2(0.2,0.1), vec2(0.2,0.9), w);
    s += segment(uv, vec2(0.2,0.9), vec2(0.6,0.9), w);
    s += segment(uv, vec2(0.6,0.7), vec2(0.6,0.9), w);
    s += segment(uv, vec2(0.6,0.7), vec2(0.75,0.5), w);
    s += segment(uv, vec2(0.6,0.3), vec2(0.75,0.5), w);
    s += segment(uv, vec2(0.6,0.1), vec2(0.6,0.3), w);
    s += segment(uv, vec2(0.2,0.1), vec2(0.6,0.1), w);
    return clamp(s, 0.0, 1.0);
}

float letterr(vec2 uv) {
    float s = 0.0;
    float w = 0.08;
    s += segment(uv, vec2(0.3,0.1), vec2(0.3,0.7), w);
    s += segment(uv, vec2(0.3,0.7), vec2(0.7,0.7), w);
    return clamp(s, 0.0, 1.0);
}
float lettera(vec2 uv) {
    float s = 0.0;
    float w = 0.08;
    s += segment(uv, vec2(0.3,0.6), vec2(0.55,0.6), w);   
    s += segment(uv, vec2(0.25,0.3), vec2(0.25,0.55), w); 
    s += segment(uv, vec2(0.3,0.25), vec2(0.55,0.25), w); 
    s += segment(uv, vec2(0.6,0.15), vec2(0.6,0.65), w);
    return clamp(s, 0.0, 1.0);
}
float letterg(vec2 uv) {
    float s = 0.0;
    float w = 0.08;
    s += segment(uv, vec2(0.3,0.7), vec2(0.6,0.7), w);  
    s += segment(uv, vec2(0.25,0.45), vec2(0.25,0.65), w); 
    s += segment(uv, vec2(0.3,0.4), vec2(0.6,0.4), w);   
    s += segment(uv, vec2(0.65,0.15), vec2(0.65,0.7), w);
    s += segment(uv, vec2(0.25,0.15), vec2(0.65,0.15), w); 
    s += segment(uv, vec2(0.25,0.15), vec2(0.25,0.25), w); 
    return clamp(s, 0.0, 1.0);
}

float lettero(vec2 uv) {
    float s = 0.0;
    float w = 0.08;
    s += segment(uv, vec2(0.25,0.6), vec2(0.7,0.6), w);
    s += segment(uv, vec2(0.25,0.2), vec2(0.25,0.6), w);
    s += segment(uv, vec2(0.7,0.2), vec2(0.7,0.6), w);
    s += segment(uv, vec2(0.25,0.2), vec2(0.7,0.2), w);
    return clamp(s, 0.0, 1.0);
}
float lettern(vec2 uv) {
    float s = 0.0;
    float w = 0.08;
    s += segment(uv, vec2(0.25,0.1), vec2(0.25,0.65), w);
    s += segment(uv, vec2(0.25,0.65), vec2(0.7,0.65), w);
    s += segment(uv, vec2(0.7,0.1), vec2(0.7,0.65), w);
    return clamp(s, 0.0, 1.0);
}
float dot_char(vec2 uv) {
    float s = 0.0;
    s += smoothstep(0.15, 0.08, length(uv - vec2(0.5, 0.15)));
    return clamp(s, 0.0, 1.0);
}
float letterL(vec2 uv) {
    float s = 0.0;
    float w = 0.08;
    s += segment(uv, vec2(0.25,0.1), vec2(0.25,0.9), w);
    s += segment(uv, vec2(0.25,0.1), vec2(0.75,0.1), w);
    return clamp(s, 0.0, 1.0);
}

void main()
{
    if(hasTexture) {
        FragColor = texture(signatureTexture, TexCoords);
    } else if(isScoreDisplay) {
        // True, display the score number in the upper left corner.
        vec2 uv = vec2(TexCoords.x, 1.0 - TexCoords.y);
        float text = 0.0;
        if(uv.x < 0.5) {
            int tens = displayScore / 10;
            if(tens > 0 || displayScore >= 10)
                text = digit(vec2(uv.x / 0.5, uv.y), tens);
        }
        else {
            int ones = displayScore % 10;
            text = digit(vec2((uv.x - 0.5) / 0.5, uv.y), ones);
        }
        
        vec3 bgColor = vec3(0.1, 0.1, 0.2);
        vec3 finalColor = mix(bgColor, textColor, text);
        FragColor = vec4(finalColor, 0.9);
    } else {
        // signature:"Dragon.L"
        vec2 uv = vec2(TexCoords.x, 1.0 - TexCoords.y);
        float text = 0.0;
        
        // Dragon.L 
        float charWidth = 1.0 / 8.0;
        
        if(uv.x < charWidth) {
            text = letterD(vec2(uv.x / charWidth, uv.y));
        } else if(uv.x < charWidth * 2.0) {
            text = letterr(vec2((uv.x - charWidth) / charWidth, uv.y));
        } else if(uv.x < charWidth * 3.0) {
            text = lettera(vec2((uv.x - charWidth * 2.0) / charWidth, uv.y));
        } else if(uv.x < charWidth * 4.0) {
            text = letterg(vec2((uv.x - charWidth * 3.0) / charWidth, uv.y));
        } else if(uv.x < charWidth * 5.0) {
            text = lettero(vec2((uv.x - charWidth * 4.0) / charWidth, uv.y));
        } else if(uv.x < charWidth * 6.0) {
            text = lettern(vec2((uv.x - charWidth * 5.0) / charWidth, uv.y));
        } else if(uv.x < charWidth * 7.0) {
            text = dot_char(vec2((uv.x - charWidth * 6.0) / charWidth, uv.y));
        } else {
            text = letterL(vec2((uv.x - charWidth * 7.0) / charWidth, uv.y));
        }
        
        vec3 bgColor = vec3(0.05, 0.05, 0.1);
        vec3 finalColor = mix(bgColor, textColor, text);
        FragColor = vec4(finalColor, 0.85);
    }
}
