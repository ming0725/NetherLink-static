#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

// Qt port of the contiguous 9-tap Gaussian from ybouane/liquidglass.
// Source: https://github.com/ybouane/liquidglass/blob/main/src/shaders.ts
uniform sampler2D u_texture;
uniform vec2 u_texelStep;
uniform vec2 u_texelSize;

varying vec2 v_texCoord;

vec4 safeTexture2D(vec2 uv)
{
    vec2 halfTexel = u_texelSize * 0.5;
    return texture2D(u_texture,
                     clamp(uv, halfTexel, vec2(1.0) - halfTexel));
}

void main()
{
    vec4 color = safeTexture2D(v_texCoord) * 0.227027;
    color += safeTexture2D(v_texCoord + u_texelStep) * 0.194594;
    color += safeTexture2D(v_texCoord - u_texelStep) * 0.194594;
    color += safeTexture2D(v_texCoord + u_texelStep * 2.0) * 0.121622;
    color += safeTexture2D(v_texCoord - u_texelStep * 2.0) * 0.121622;
    color += safeTexture2D(v_texCoord + u_texelStep * 3.0) * 0.054054;
    color += safeTexture2D(v_texCoord - u_texelStep * 3.0) * 0.054054;
    color += safeTexture2D(v_texCoord + u_texelStep * 4.0) * 0.016216;
    color += safeTexture2D(v_texCoord - u_texelStep * 4.0) * 0.016216;
    gl_FragColor = color;
}
