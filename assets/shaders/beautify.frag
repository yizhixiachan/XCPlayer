#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    bool dynamicEffect;
    float time;
    vec4 color1;
    vec4 color2;
} ubuf;

layout(binding = 1) uniform sampler2D sourceMap;

// RGB 转 HSV
vec3 RGB2HSV(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSV 转 RGB
vec3 HSV2RGB(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// 随机函数
float Random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

// 动态扭曲
vec2 GetOffset(vec2 uv, float t) {
    // --- 基础呼吸/摇摆 ---
    float BASE_SWAY_SPEED = 0.36;          // 整体摇摆的基础速度（越大越快）
    float BASE_SWAY_AMPLITUDE_X = 0.06;    // 左右摇摆最大幅度（0.0=不动，0.1=10%画面宽度）
    float BASE_SWAY_AMPLITUDE_Y = 0.04;    // 上下摇摆最大幅度（0.0=不动，0.1=10%画面高度）
    float SUB_SWAY_AMPLITUDE = 0.018;      // 次级不规则扰动的幅度（叠加在主摇摆上的小抖动）

    // --- 形变折痕 ---
    float WARP_FOLD_COUNT = 2.0;           // 形变"折痕"数量（越小形变越宽广整体，越大越细碎）
    float WARP_FOLD_SPEED_X = 0.4;         // X方向折痕翻滚速度
    float WARP_FOLD_SPEED_Y = 0.3;         // Y方向折痕翻滚速度
    float WARP_FOLD_STRENGTH = 0.01;       // 折痕扭曲力度（0.0=无形变，越大拉扯越强）

    // --- 阵风 ---
    float GUST_FREQUENCY = 0.7;            // 阵风频率
    float GUST_SHARPNESS = 3.0;            // 阵风锐度（越大平静期越长、爆发越短促，越小越平缓）
    float GUST_WAVE1_STRENGTH = 0.6;       // 第一道风力波强度
    float GUST_WAVE2_STRENGTH = 0.5;       // 第二道风力波强度
    float GUST_WAVE2_FREQ_MULT = 2.3;      // 第二道风力波频率倍数（相对于GUST_FREQUENCY）

    // --- 风扫过方向 ---
    float WIND_SWEEP_DENSITY = 5.0;        // 风扫遮罩的密度（uv.x*5.0 + uv.y*5.0，数字越大条纹越密）
    float WIND_SWEEP_SPEED = 2.5;          // 风扫过画面的速度
    float WIND_SWEEP_CONTRAST = 0.5;       // 风扫遮罩的对比度（0.0=均匀，1.0=强烈明暗）

    // --- 水波纹 ---
    float RIPPLE_DENSITY_X = 60.0;         // X轴水波纹密度（数字越大波纹越细密，越小越宽大）
    float RIPPLE_SPEED_X = 6.0;            // X轴水波纹流动速度
    float RIPPLE_STRENGTH_X = 0.006;       // X轴水波纹折射扭曲力度（0.0=无波纹）
    float RIPPLE_DENSITY_Y = 50.0;         // 辅轴波纹X方向密度
    float RIPPLE_DENSITY_Y2 = 20.0;        // 辅轴波纹Y方向密度（与X混合产生纵横交错感）
    float RIPPLE_SPEED_Y = 5.0;            // 辅轴波纹流动速度
    float RIPPLE_STRENGTH_Y = 0.004;       // 辅轴波纹折射扭曲力度

    float panT = t * BASE_SWAY_SPEED; 

    float mainWave = sin(panT); 

    // 左右/上下基础摇摆 + 次级抖动
    float moveX = mainWave * BASE_SWAY_AMPLITUDE_X + cos(panT * 0.73) * SUB_SWAY_AMPLITUDE; 
    float moveY = mainWave * BASE_SWAY_AMPLITUDE_Y + sin(panT * 0.89) * SUB_SWAY_AMPLITUDE;

    // 形变折痕
    float warpX = sin(uv.y * WARP_FOLD_COUNT + t * WARP_FOLD_SPEED_X) * WARP_FOLD_STRENGTH;
    float warpY = cos(uv.x * WARP_FOLD_COUNT - t * WARP_FOLD_SPEED_Y) * WARP_FOLD_STRENGTH;

    // 阵风生成
    float gustTime = t * GUST_FREQUENCY;
    float windGust = max(0.0, sin(gustTime) * GUST_WAVE1_STRENGTH + sin(gustTime * GUST_WAVE2_FREQ_MULT) * GUST_WAVE2_STRENGTH);
    windGust = pow(windGust, GUST_SHARPNESS); 

    // 风扫遮罩
    float windSweep = sin(uv.x * WIND_SWEEP_DENSITY + uv.y * WIND_SWEEP_DENSITY - t * WIND_SWEEP_SPEED) * WIND_SWEEP_CONTRAST + (1.0 - WIND_SWEEP_CONTRAST);
    float activeWind = windGust * windSweep; // 最终风力遮罩

    // 水波纹
    float rippleX = sin(uv.y * RIPPLE_DENSITY_X - t * RIPPLE_SPEED_X) * RIPPLE_STRENGTH_X * activeWind;
    float rippleY = cos(uv.x * RIPPLE_DENSITY_Y + uv.y * RIPPLE_DENSITY_Y2 - t * RIPPLE_SPEED_Y) * RIPPLE_STRENGTH_Y * activeWind;

    // 最终合成
    return vec2(moveX + warpX + rippleX, moveY + warpY + rippleY);
}

void main() {
    vec2 uv = qt_TexCoord0;
    
    // 获取扭曲后的坐标
    vec2 distortedUV = ubuf.dynamicEffect ? uv + GetOffset(uv, ubuf.time) : uv;

  // X轴左右各裁掉 6%，Y轴上下各裁掉 4% 仿照超出边界
    vec2 safeMargin = vec2(0.06, 0.04); 
    
    // 将扭曲后的UV映射到原图的内部安全区域
    vec2 safeUV = distortedUV * (1.0 - 2.0 * safeMargin) + safeMargin;

    // 采样原图
    vec4 baseColor = texture(sourceMap, safeUV);

    if(ubuf.dynamicEffect) {
        // 转换纯色
        vec3 hsv1 = RGB2HSV(ubuf.color1.rgb);
        hsv1.y = 0.5; hsv1.z = 1.0; 
        vec3 pureColor1 = HSV2RGB(hsv1);

        vec3 hsv2 = RGB2HSV(ubuf.color2.rgb);
        hsv2.y = 0.5; hsv2.z = 1.0;
        vec3 pureColor2 = HSV2RGB(hsv2);

        float dynamicAngle = ubuf.time * 0.05                     // 基础缓慢旋转
                           + sin(ubuf.time * 0.12) * 1.5          // 主摇摆
                           + cos(ubuf.time * 0.07) * 1.2;         // 次级不规则扰动
        vec2 angleDir = vec2(cos(dynamicAngle), sin(dynamicAngle));
    
        // 构造一个平滑的、随时间流动的2D空间扭曲场
        float warpX = sin(uv.y * 3.5 + ubuf.time * 0.4) * cos(uv.x * 2.5 - ubuf.time * 0.3);
        float warpY = cos(uv.x * 3.5 - ubuf.time * 0.5) * sin(uv.y * 2.5 + ubuf.time * 0.4);
    
        // 将原 UV 加上扭曲量，0.4 是边缘扭曲的强度（越大曲线越夸张）
        vec2 warpedUV = uv + vec2(warpX, warpY) * 0.4;

        // 点乘将 "扭曲后的UV" 投影到斜线上
        float projectedUV = dot(warpedUV, angleDir);
    
        // 生成缓慢推进的波浪形渐变因子
        // projectedUV系数：渐变条纹数量，ubuf.time系数: 渐变流速
        float gradPhase = projectedUV * 3.0 - ubuf.time * 0.10;
        float gradFactor = sin(gradPhase) * 0.5 + 0.5;    
        vec3 fogGradientColor = mix(pureColor1, pureColor2, gradFactor);
    
        // 雾气底层
        float mistBase = sin(uv.x * 4.0 + ubuf.time * 0.30) * cos(uv.y * 4.0 - ubuf.time * 0.50);
        // 雾气细节
        float mistDetail = sin(uv.x * 12.0 - ubuf.time * 0.80) * cos(uv.y * 12.0 + ubuf.time * 1.00);
        // 混合两层雾气，并将其限制在 0.0 ~ 1.0 之间
        float mistThick = (mistBase * 0.7 + mistDetail * 0.3) * 0.5 + 0.5;
    
        // 计算原图当前的亮度
        float baseLuminance = dot(baseColor.rgb, vec3(0.299, 0.587, 0.114));
        // 生成基于亮度的雾气权重
        float luminanceMask = mix(0.2, 0.5, baseLuminance);
        // 计算最终的 fogAlpha
        float fogAlpha = 1.2 * mistThick * luminanceMask;
        // 限制最高浓度
        fogAlpha = clamp(fogAlpha, 0.1, 0.4);

        // 将带有雾气的渐变叠加到水波纹原图上
        baseColor.rgb = mix(baseColor.rgb, fogGradientColor, fogAlpha);
    }
    
    // 调色
    vec4 finalColor = baseColor;
    float luminance = dot(finalColor.rgb, vec3(0.299, 0.587, 0.114));
    finalColor.rgb = mix(vec3(luminance), finalColor.rgb, 1.3);
    finalColor.rgb = pow(finalColor.rgb, vec3(1.2));        
    finalColor.rgb *= 0.6;                                      

    vec2 noiseCoord = gl_FragCoord.xy + (ubuf.time * 50.0);
    float noise = (Random(noiseCoord) - 0.5) * 0.004;
    finalColor.rgb += noise * finalColor.a; 
    
    // 输出
    fragColor = finalColor * ubuf.qt_Opacity;
}