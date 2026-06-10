#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneTex;
uniform sampler2D normalTex;
uniform sampler2D depthTex;
uniform sampler2D albedoTex;
uniform mat4 projection;
uniform mat4 invProjection;
uniform float time;

const int   SSGI_SAMPLES   = 8;
const int   SSGI_STEPS     = 12;
const float SSGI_STEP_SIZE = 0.35;
const float SSGI_THICKNESS = 0.6;
const float SSGI_STRENGTH  = 1.5;

// 8 unit-length directions distributed across the upper hemisphere.
// z is the NdotL weight; rotation around Z never changes z, so we
// can use s.z directly instead of recomputing dot(dir, N) each sample.
const vec3 hemiSamples[8] = vec3[8](
    vec3( 0.000,  0.000,  1.000),
    vec3( 0.894,  0.000,  0.447),
    vec3( 0.276,  0.851,  0.447),
    vec3(-0.724,  0.526,  0.447),
    vec3(-0.724, -0.526,  0.447),
    vec3( 0.276, -0.851,  0.447),
    vec3( 0.500,  0.500,  0.707),
    vec3(-0.500,  0.500,  0.707)
);

vec3 ReconstructViewPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjection * clip;
    return view.xyz / view.w;
}

float hash(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

// Build a TBN so that tangent-space Z maps to the view-space normal N
mat3 buildTBN(vec3 N) {
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);
    return mat3(T, B, N);
}

vec3 RayMarchGI(vec3 P, vec3 dir) {
    vec3 stepVec = dir * SSGI_STEP_SIZE;
    vec3 rayPos  = P + stepVec;

    for (int i = 0; i < SSGI_STEPS; i++) {
        vec4 clip = projection * vec4(rayPos, 1.0);
        if (clip.w <= 0.0) break;
        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;

        float sceneDepth = texture(depthTex, uv).r;
        if (sceneDepth >= 1.0) { rayPos += stepVec; continue; }

        vec3  scenePos = ReconstructViewPos(uv, sceneDepth);
        float delta    = rayPos.z - scenePos.z;

        if (delta < 0.0 && delta > -SSGI_THICKNESS) {
            // Quadratic distance attenuation so near bounces are stronger
            float dist    = float(i + 1) * SSGI_STEP_SIZE;
            float maxDist = float(SSGI_STEPS) * SSGI_STEP_SIZE;
            float atten   = 1.0 - clamp(dist / maxDist, 0.0, 1.0);
            return texture(sceneTex, uv).rgb * (atten * atten);
        }
        rayPos += stepVec;
    }
    return vec3(0.0);
}

void main() {
    float depth = texture(depthTex, TexCoords).r;
    if (depth >= 1.0) {
        // Sky — no geometry to receive GI
        FragColor = vec4(0.0);
        return;
    }

    vec4 normalRoughness = texture(normalTex, TexCoords);
    vec3 N      = normalize(normalRoughness.rgb * 2.0 - 1.0);
    vec3 P      = ReconstructViewPos(TexCoords, depth);
    vec3 albedo = texture(albedoTex, TexCoords).rgb;

    mat3 TBN = buildTBN(N);

    // Per-pixel rotation around the surface normal to break up structured banding
    float angle = hash(TexCoords + vec2(time * 0.1)) * 6.28318;
    float cosA  = cos(angle);
    float sinA  = sin(angle);

    vec3 indirectLight = vec3(0.0);
    for (int i = 0; i < SSGI_SAMPLES; i++) {
        vec3 s = hemiSamples[i];
        // Rotate in tangent-space XY (Z = normal stays unchanged)
        s = vec3(s.x * cosA - s.y * sinA,
                 s.x * sinA + s.y * cosA,
                 s.z);

        vec3  dir    = normalize(TBN * s);
        float NdotL  = s.z; // invariant: dot(TBN*s, N) == s.z for any TBN built from N
        indirectLight += RayMarchGI(P, dir) * NdotL;
    }

    // Normalize, apply Lambertian albedo (color bleeding), and scale
    indirectLight = (indirectLight / float(SSGI_SAMPLES)) * albedo * SSGI_STRENGTH;
    FragColor = vec4(max(indirectLight, vec3(0.0)), 1.0);
}
