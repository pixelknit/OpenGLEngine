#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneTex;   // linear HDR scene color
uniform sampler2D normalTex;  // view-space normals (encoded) + roughness in alpha
uniform sampler2D depthTex;   // scene depth
uniform sampler2D ssgiTex;    // indirect diffuse from SSGI pass
uniform sampler2D rsmTex;     // indirect diffuse from RSM pass

uniform mat4 projection;
uniform mat4 invProjection;

const int   SSR_STEPS        = 32;
const int   SSR_BINARY_STEPS = 8;
const float SSR_STEP_SIZE    = 0.3;
const float SSR_THICKNESS    = 0.5;

vec3 ReconstructViewPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjection * clip;
    return view.xyz / view.w;
}

// Returns screen UV of hit, or vec2(-1.0) on miss
vec2 RayMarch(vec3 P, vec3 R) {
    vec3 step   = R * SSR_STEP_SIZE;
    vec3 rayPos = P + step;

    for (int i = 0; i < SSR_STEPS; i++) {
        vec4 clip = projection * vec4(rayPos, 1.0);
        if (clip.w <= 0.0) break;
        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;

        float sceneDepth = texture(depthTex, uv).r;
        if (sceneDepth >= 1.0) { rayPos += step; continue; }

        vec3 scenePos = ReconstructViewPos(uv, sceneDepth);
        float delta   = rayPos.z - scenePos.z;

        // Ray just passed behind the surface
        if (delta < 0.0 && delta > -SSR_THICKNESS) {
            vec3 lo = rayPos - step, hi = rayPos;
            for (int j = 0; j < SSR_BINARY_STEPS; j++) {
                vec3 mid      = (lo + hi) * 0.5;
                vec4 midClip  = projection * vec4(mid, 1.0);
                if (midClip.w <= 0.0) break;
                vec2 midUV    = (midClip.xy / midClip.w) * 0.5 + 0.5;
                vec3 midScene = ReconstructViewPos(midUV, texture(depthTex, midUV).r);
                if (mid.z < midScene.z) hi = mid; else lo = mid;
            }
            vec4 finalClip = projection * vec4((lo + hi) * 0.5, 1.0);
            return (finalClip.xy / finalClip.w) * 0.5 + 0.5;
        }
        rayPos += step;
    }
    return vec2(-1.0);
}

void main() {
    vec3  sceneColor     = texture(sceneTex, TexCoords).rgb;
    float depth          = texture(depthTex, TexCoords).r;
    vec4  normalRoughness = texture(normalTex, TexCoords);
    float roughness      = normalRoughness.a;

    // Only apply SSR to smooth surfaces (roughness < ~0.33)
    float ssrStrength = clamp(1.0 - roughness * 3.0, 0.0, 1.0);
    ssrStrength *= ssrStrength;

    vec3 finalColor = sceneColor;

    if (ssrStrength > 0.01 && depth < 1.0) {
        vec3 N = normalize(normalRoughness.rgb * 2.0 - 1.0);
        vec3 P = ReconstructViewPos(TexCoords, depth);
        vec3 V = normalize(-P);
        vec3 R = normalize(reflect(-V, N));

        vec2 hitUV = RayMarch(P, R);
        if (hitUV.x >= 0.0) {
            vec3  hitColor = texture(sceneTex, hitUV).rgb;
            vec2  edgeDist = abs(hitUV * 2.0 - 1.0);
            float edgeFade = 1.0 - smoothstep(0.7, 1.0, max(edgeDist.x, edgeDist.y));
            finalColor = mix(sceneColor, hitColor, ssrStrength * edgeFade);
        }
    }

    // Add indirect diffuse — both screen-space (SSGI) and sun-bounce (RSM)
    finalColor += texture(ssgiTex, TexCoords).rgb;
    finalColor += texture(rsmTex,  TexCoords).rgb;

    // ACES filmic tone mapping + gamma
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    finalColor = clamp((finalColor*(a*finalColor+b))/(finalColor*(c*finalColor+d)+e), 0.0, 1.0);
    finalColor = pow(finalColor, vec3(1.0 / 2.2));
    FragColor  = vec4(finalColor, 1.0);
}
