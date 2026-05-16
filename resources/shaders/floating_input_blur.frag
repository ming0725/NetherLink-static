#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D u_texture;
uniform vec2 u_texelStep;

varying vec2 v_texCoord;

vec4 safeTexture2D(sampler2D tex, vec2 uv)
{
    return texture2D(tex, clamp(uv, vec2(0.001), vec2(0.999)));
}

vec3 adjustSaturation(vec3 color, float saturation)
{
    float gray = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(gray), color, saturation);
}

void main()
{
    vec2 step1 = u_texelStep * 1.3846153846;
    vec2 step2 = u_texelStep * 3.2307692308;

    vec4 color = safeTexture2D(u_texture, v_texCoord) * 0.2270270270;

    color += safeTexture2D(u_texture, v_texCoord + step1) * 0.3162162162;
    color += safeTexture2D(u_texture, v_texCoord - step1) * 0.3162162162;

    color += safeTexture2D(u_texture, v_texCoord + step2) * 0.0702702703;
    color += safeTexture2D(u_texture, v_texCoord - step2) * 0.0702702703;

    vec3 result = color.rgb;

    result = (result - 0.5) * 0.92 + 0.5;
    result *= 1.08;
    result = adjustSaturation(result, 1.15);

    gl_FragColor = vec4(result, color.a);
}
