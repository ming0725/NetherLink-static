#ifdef GL_ES
precision highp float;
#endif

attribute vec2 a_position;
attribute vec2 a_texCoord;

varying vec2 v_texCoord;

void main()
{
    v_texCoord = a_texCoord;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
