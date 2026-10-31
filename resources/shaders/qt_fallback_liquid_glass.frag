#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

// Qt port of the biconvex pill model from ybouane/liquidglass.
// Source: https://github.com/ybouane/liquidglass/blob/main/src/shaders.ts
uniform sampler2D u_sourceTexture;
uniform sampler2D u_softTexture;
uniform vec2 u_size;
uniform vec4 u_shapeRect;
uniform float u_radius;
uniform float u_shapeInset;
uniform float u_pixelScale;
uniform float u_dark;

varying vec2 v_texCoord;

// Material controls. Frost comes from the GPU-blurred transmission texture;
// surface lighting below follows the actual biconvex bevel normals.
const float kFrostAmount = 0.94;
const float kFrostGrain = 0.004;
const float kRefraction = 0.58;

float roundedRectSdf(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + vec2(radius);
    return min(max(q.x, q.y), 0.0)
            + length(max(q, vec2(0.0)))
            - radius;
}

float bevelHeight(float distanceInside, float zRadius)
{
    if (distanceInside <= 0.0) {
        return 0.0;
    }
    if (distanceInside >= zRadius) {
        return zRadius;
    }
    return sqrt(distanceInside * (2.0 * zRadius - distanceInside));
}

vec2 clampUv(vec2 uv)
{
    vec2 halfTexel = 0.5 / max(u_size, vec2(1.0));
    return clamp(uv, halfTexel, vec2(1.0) - halfTexel);
}

void main()
{
    vec2 shapeSize = max(u_shapeRect.zw, vec2(1.0));
    vec2 shapeCenter = u_shapeRect.xy + shapeSize * 0.5;
    vec2 localPx = v_texCoord * u_size - shapeCenter;
    vec2 halfSize = max(shapeSize * 0.5 - vec2(u_shapeInset), vec2(0.5));
    float maxRadius = max(0.0, min(halfSize.x, halfSize.y) - 0.5);
    float radius = min(max(u_radius, 0.0), maxRadius);
    float sdf = roundedRectSdf(localPx, halfSize, radius);
    float inside = -sdf;
    float maxDistance = min(halfSize.x, halfSize.y);
    // The WebGL source writes reversed smoothstep edges. Use its defined
    // algebraic equivalent so desktop GLSL and GLES drivers agree.
    float edge = 1.0 - smoothstep(0.0, maxDistance * 0.35, inside);

    // The reference defaults pair cornerRadius=65 with zRadius=40. Qt panels
    // commonly use much smaller 15-20 DIP corners, so preserve that ratio;
    // keeping a fixed 40 DIP depth makes the four corner bevels overlap.
    float referenceZRatio = 40.0 / 65.0;
    float zRadius = max(1.0,
                        min(min(40.0 * u_pixelScale,
                                radius * referenceZRatio),
                            maxDistance * 0.92));
    float epsilon = max(1.0, u_pixelScale);
    float distanceRight = -roundedRectSdf(localPx + vec2(epsilon, 0.0), halfSize, radius);
    float distanceLeft = -roundedRectSdf(localPx - vec2(epsilon, 0.0), halfSize, radius);
    float distanceUp = -roundedRectSdf(localPx + vec2(0.0, epsilon), halfSize, radius);
    float distanceDown = -roundedRectSdf(localPx - vec2(0.0, epsilon), halfSize, radius);
    float centerHeight = bevelHeight(inside, zRadius);
    float heightRight = bevelHeight(distanceRight, zRadius);
    float heightLeft = bevelHeight(distanceLeft, zRadius);
    float heightUp = bevelHeight(distanceUp, zRadius);
    float heightDown = bevelHeight(distanceDown, zRadius);
    vec2 heightGradient = vec2(heightRight - heightLeft,
                               heightUp - heightDown) / (2.0 * epsilon);
    vec3 surfaceNormal = normalize(vec3(-heightGradient, 1.0));
    float depth = smoothstep(0.0, zRadius, inside);

    float refraction = kRefraction;
    float refractionPower = 1.0 - 1.0 / 1.5;
    float thickness = centerHeight * 2.0;
    float normalizedThickness = thickness / max(zRadius * 2.0, 1.0);
    vec2 entryRefraction = heightGradient * refractionPower;
    vec2 exitRefraction = heightGradient * refractionPower;
    vec2 throughRefraction = entryRefraction * normalizedThickness * 0.5;
    vec2 refractionPx = (entryRefraction
                         + exitRefraction
                         + throughRefraction) * refraction * 25.5;
    vec2 centerDirection = -localPx / max(halfSize, vec2(1.0));
    refractionPx += centerDirection * refraction * 4.0 * depth;

    // Keep the bevel curvature unchanged, but let its rim look farther into
    // the surrounding scene. A normalized offset extends sampling reach
    // without multiplying the height gradient that shapes the four corners.
    float refractionLength = length(refractionPx);
    vec2 refractionDirection = refractionLength > 0.0001
            ? refractionPx / refractionLength
            : vec2(0.0);
    float rimReach = 1.0 - smoothstep(0.0,
                                      max(zRadius, 1.0),
                                      max(inside, 0.0));
    refractionPx += refractionDirection
            * rimReach * (8.0 * u_pixelScale);
    vec2 baseUv = clampUv(v_texCoord + refractionPx / u_size);

    // Keep dispersion restrained: the material should read as frosted glass,
    // with only a little clearer transmission along the curved rim.
    float chromaPixels = 0.025 * 18.0 * (edge * 0.7 + 0.3) * u_pixelScale;
    vec2 chromaUv = surfaceNormal.xy * chromaPixels / u_size;
    vec3 sharp = vec3(
        texture2D(u_sourceTexture, clampUv(baseUv + chromaUv)).r,
        texture2D(u_sourceTexture, baseUv).g,
        texture2D(u_sourceTexture, clampUv(baseUv - chromaUv)).b
    );
    vec3 blurred = vec3(
        texture2D(u_softTexture, clampUv(baseUv + chromaUv)).r,
        texture2D(u_softTexture, baseUv).g,
        texture2D(u_softTexture, clampUv(baseUv - chromaUv)).b
    );
    vec3 color = mix(sharp, blurred, kFrostAmount - edge * 0.10);
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, 0.90);

    // A lightly smoked body absorbs more light through its thicker center.
    float absorption = mix(0.025, 0.045, u_dark) * (0.65 + 0.35 * normalizedThickness);
    vec3 smokeColor = mix(vec3(0.07, 0.085, 0.105), vec3(0.018, 0.024, 0.034), u_dark);
    color = mix(color, smokeColor, absorption);
    // Lift transmission slightly while retaining detail near white.
    color += (vec3(1.0) - color) * mix(0.025, 0.035, u_dark);

    // Stationary, low-amplitude micrograin keeps the frost from looking like
    // an ordinary blur. Anchor it in DIP; no time uniform or frame flicker.
    vec2 grainCell = floor(localPx / max(u_pixelScale, 0.001));
    float grain = fract(sin(dot(grainCell, vec2(127.1, 311.7))) * 43758.5453) - 0.5;
    color += vec3(grain * kFrostGrain);

    float bevel = 1.0 - smoothstep(0.0, zRadius, max(inside, 0.0));
    float curvature = 1.0 - clamp(surfaceNormal.z, 0.0, 1.0);
    float fresnel = 0.04 + 0.96 * pow(curvature, 5.0);

    // Diffuse ambient lighting treats all four edges equally. Keep only a
    // subtle curvature response, with no directional specular or bright lip.
    color *= 1.0 - bevel * curvature * 0.02;
    color = mix(color, vec3(0.76, 0.80, 0.86), fresnel * bevel * 0.025);

    // QPainter owns the rounded antialiased clip, so the texture stays opaque.
    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
