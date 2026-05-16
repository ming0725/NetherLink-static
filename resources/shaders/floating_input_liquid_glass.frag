#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D u_blurTexture;
uniform vec2 u_size;
uniform float u_radius;
uniform float u_shapeInset;
uniform float u_dark;

varying vec2 v_texCoord;

float roundedBoxSdf(vec2 p, vec2 halfSize, float radius)
{
    vec2 innerHalfSize = halfSize - vec2(radius);
    vec2 q = abs(p) - innerHalfSize;
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

vec2 roundedBoxNormal(vec2 p, vec2 halfSize, float radius)
{
    float h = 1.25;
    vec2 gradient = vec2(
        roundedBoxSdf(p + vec2(h, 0.0), halfSize, radius) - roundedBoxSdf(p - vec2(h, 0.0), halfSize, radius),
        roundedBoxSdf(p + vec2(0.0, h), halfSize, radius) - roundedBoxSdf(p - vec2(0.0, h), halfSize, radius)
    );
    return normalize(gradient + vec2(0.00001));
}

void main()
{
    vec2 uv = v_texCoord;
    vec2 p = (uv - 0.5) * u_size;
    vec2 halfSize = max(u_size * 0.5 - vec2(u_shapeInset), vec2(0.5));
    float maxRadius = max(0.0, min(halfSize.x, halfSize.y) - 0.5);
    float radius = min(max(u_radius, 0.0), maxRadius);
    float sdf = roundedBoxSdf(p, halfSize, radius);
    float insideDistance = max(-sdf, 0.0);
    vec2 normal = roundedBoxNormal(p, halfSize, radius);
    vec2 tangent = vec2(normal.y, -normal.x);

    float minSize = min(u_size.x, u_size.y);
    float lensWidth = max(12.0, min(minSize * 0.34, max(radius * 1.45, minSize * 0.16)));
    float normalizedDistance = insideDistance / max(lensWidth, 1.0);
    float edgeFalloff = 1.0 - smoothstep(0.0, 1.0, normalizedDistance);
    float lensAmount = pow(edgeFalloff, 1.85);
    float innerRidge = exp(-pow((insideDistance - lensWidth * 0.34) / max(1.0, lensWidth * 0.16), 2.0));
    float cornerAmount = smoothstep(0.18, 0.58, abs(normal.x * normal.y) * 2.0);

    float wave = sin(dot(uv, vec2(33.0, 41.0)) + normal.x * 3.4 - normal.y * 2.8);
    float bendPixels = (minSize * 0.052 + radius * 0.34) * lensAmount * mix(1.0, 1.32, cornerAmount);
    bendPixels += radius * 0.08 * innerRidge;
    float tangentPixels = (1.6 + radius * 0.075) * wave * lensAmount * (0.28 + 0.72 * innerRidge);
    tangentPixels += cornerAmount * innerRidge * radius * 0.08 * sign(wave);
    float outwardStretch = pow(edgeFalloff, 1.18);
    float horizontalEdge = smoothstep(0.36, 0.92, abs(normal.y));
    float horizontalStretchPixels = (minSize * 0.032 + radius * 0.16)
            * outwardStretch
            * mix(0.34, 1.0, horizontalEdge)
            * mix(1.0, 1.22, cornerAmount);
    vec2 horizontalStretch = vec2(-(p.x / max(halfSize.x, 1.0)) * horizontalStretchPixels, 0.0);
    vec2 refract = (-normal * bendPixels + tangent * tangentPixels + horizontalStretch) / u_size;

    vec2 texel = 1.0 / max(u_size, vec2(1.0));
    vec2 refractedUv = clamp(uv + refract, texel * 0.75, vec2(1.0) - texel * 0.75);
    vec3 color = texture2D(u_blurTexture, refractedUv).rgb;
    color = mix(color, vec3(1.0), mix(0.030, 0.008, u_dark));

    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
