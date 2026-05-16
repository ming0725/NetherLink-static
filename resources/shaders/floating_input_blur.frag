#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D u_texture;
uniform vec2 u_texelStep;

varying vec2 v_texCoord;

void main()
{
    vec4 color = texture2D(u_texture, v_texCoord) * 0.19648255;
    color += texture2D(u_texture, v_texCoord + u_texelStep * 1.41176471) * 0.29690696;
    color += texture2D(u_texture, v_texCoord - u_texelStep * 1.41176471) * 0.29690696;
    color += texture2D(u_texture, v_texCoord + u_texelStep * 3.29411765) * 0.09447040;
    color += texture2D(u_texture, v_texCoord - u_texelStep * 3.29411765) * 0.09447040;
    color += texture2D(u_texture, v_texCoord + u_texelStep * 5.17647059) * 0.01038136;
    color += texture2D(u_texture, v_texCoord - u_texelStep * 5.17647059) * 0.01038136;
    gl_FragColor = color;
}
