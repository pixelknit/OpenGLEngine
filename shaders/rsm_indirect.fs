#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneNormalTex;  // view-space normals + roughness
uniform sampler2D sceneDepthTex;   // scene depth
uniform sampler2D rsmPosTex;       // RSM world-space positions
uniform sampler2D rsmNormalTex;    // RSM world-space normals
uniform sampler2D rsmFluxTex;      // RSM reflected flux (albedo * sunColor)

uniform mat4 invProjection;
uniform mat4 invView;
uniform mat4 lightSpaceMatrix;

// Tuning constants
const int   RSM_SAMPLES        = 32;
const float RSM_SAMPLE_RADIUS  = 0.08;  // fraction of RSM UV space (~5 world units for ±35 ortho)
const float RSM_STRENGTH       = 0.4;
const float RSM_MIN_DIST       = 0.5;   // skip VPLs closer than this (avoids self-contribution singularity)
const float RSM_MAX_CONTRIB    = 8.0;   // per-sample HDR cap — catches any remaining outliers

vec3 ReconstructViewPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjection * clip;
    return view.xyz / view.w;
}

// Reversed-bits Van der Corput sequence — provides xi2 for Hammersley sampling
float VanDerCorput(uint n) {
    n = (n << 16u) | (n >> 16u);
    n = ((n & 0x55555555u) << 1u) | ((n & 0xAAAAAAAAu) >> 1u);
    n = ((n & 0x33333333u) << 2u) | ((n & 0xCCCCCCCCu) >> 2u);
    n = ((n & 0x0F0F0F0Fu) << 4u) | ((n & 0xF0F0F0F0u) >> 4u);
    n = ((n & 0x00FF00FFu) << 8u) | ((n & 0xFF00FF00u) >> 8u);
    return float(n) * 2.3283064365386963e-10;
}

float hash(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

void main() {
    float depth = texture(sceneDepthTex, TexCoords).r;
    if (depth >= 1.0) {
        FragColor = vec4(0.0);
        return;
    }

    // Reconstruct world-space position and normal from G-buffer
    vec3 P_view  = ReconstructViewPos(TexCoords, depth);
    vec3 P_world = (invView * vec4(P_view, 1.0)).xyz;

    vec4 normalRoughness = texture(sceneNormalTex, TexCoords);
    vec3 N_view  = normalize(normalRoughness.rgb * 2.0 - 1.0);
    vec3 N_world = normalize(mat3(invView) * N_view);

    // Project current world position into the light's RSM texture space
    vec4 lsPos   = lightSpaceMatrix * vec4(P_world, 1.0);
    vec2 centerUV = (lsPos.xy / lsPos.w) * 0.5 + 0.5;

    // Points outside the shadow frustum receive no RSM contribution
    if (centerUV.x < 0.0 || centerUV.x > 1.0 ||
        centerUV.y < 0.0 || centerUV.y > 1.0) {
        FragColor = vec4(0.0);
        return;
    }

    // Per-pixel rotation so the sample pattern doesn't align to a grid
    float rot = hash(TexCoords) * 6.28318;

    vec3 indirect = vec3(0.0);

    for (uint i = 0u; i < uint(RSM_SAMPLES); i++) {
        // Hammersley 2D point on unit disk, then rotate and scale to RSM UV space
        float xi1   = float(i) / float(RSM_SAMPLES);
        float xi2   = VanDerCorput(i);
        float r     = sqrt(xi1) * RSM_SAMPLE_RADIUS;  // sqrt gives uniform area distribution
        float theta = xi2 * 6.28318 + rot;
        vec2  sampleUV = centerUV + vec2(r * cos(theta), r * sin(theta));

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;

        vec3 vpl_pos  = texture(rsmPosTex,   sampleUV).rgb;
        vec3 vpl_norm = normalize(texture(rsmNormalTex, sampleUV).rgb);
        vec3 vpl_flux = texture(rsmFluxTex,  sampleUV).rgb;

        // Skip RSM texels with no geometry (cleared to black)
        if (dot(vpl_flux, vpl_flux) < 0.001) continue;

        vec3  toSurf = P_world - vpl_pos;
        float dist   = length(toSurf);
        // Skip VPLs that are part of the same or adjacent surface — the main source
        // of artifacts. dist^4 diverges faster than any clamp can suppress cleanly.
        if (dist < RSM_MIN_DIST) continue;
        vec3 dir = toSurf / dist;

        // Bidirectional geometry term: VPL emits toward surface, surface receives from VPL
        float NdotL_vpl  = max(dot(vpl_norm,  dir),  0.0);
        float NdotL_surf = max(dot(N_world,  -dir),  0.0);

        float dist4  = dist * dist * dist * dist;
        vec3  contrib = vpl_flux * (NdotL_vpl * NdotL_surf) / dist4;
        // Cap each sample so a single outlier can't dominate the result
        indirect += min(contrib, vec3(RSM_MAX_CONTRIB));
    }

    indirect *= RSM_STRENGTH / float(RSM_SAMPLES);
    FragColor = vec4(max(indirect, vec3(0.0)), 1.0);
}
