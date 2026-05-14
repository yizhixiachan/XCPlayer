#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 offset; 
    float radius;
} ubuf;

layout(binding = 1) uniform sampler2D sourceMap;

void main() {
    vec2 uv = qt_TexCoord0;
    float r = ubuf.radius;
    
    if(r <= 0.0) {
        fragColor = texture(sourceMap, uv) * ubuf.qt_Opacity;
        return;
    }

    float t = r / 100.0;
    float sigma = t * t * 100.0; 
    
    float twoSigma2 = 2.0 * sigma * sigma;

    float sampleRadius = sigma * 3.5;
    float step = max(0.3, sampleRadius / 100.0);
    
    vec2 offsetVec = ubuf.offset;
    
    vec4 accum = vec4(0.0);
    float totalWeight = 0.0;
    
    for(float i = -sampleRadius; i <= sampleRadius; i += step) {
        float weight = exp(-(i * i) / twoSigma2);
        vec2 sampleUV = clamp(uv + offsetVec * i, 0.0, 1.0);
        
        accum += texture(sourceMap, sampleUV) * weight;
        totalWeight += weight;
    }
    
    vec4 blurred = accum / totalWeight;
    fragColor = blurred * ubuf.qt_Opacity;
}