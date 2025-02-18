$input v_texcoord0

#include <bgfx_shader.sh>

uniform float u_useTexture;
SAMPLER2D(s_texY, 0);
SAMPLER2D(s_texU, 1);
SAMPLER2D(s_texV, 2);




void main() {
    if(u_useTexture < 1.0) {
        gl_FragColor = vec4(0.2, 0.4, 0.8, 1.0);
    } else {
    //YUV to RGB
    float y = texture2D(s_texY, v_texcoord0.xy).x;
    float u = texture2D(s_texU, v_texcoord0.xy).x - 0.5;
    float v = texture2D(s_texV, v_texcoord0.xy).x - 0.5;
        vec3 yuv = vec3(y, u, v);
        vec3 rgb = mul(yuv, mat3(1.0, 1.0, 1.0, 0.0, -0.39465, 2.03211, 1.13983, -0.58060, 0.0) );
        gl_FragColor = vec4(rgb.x, rgb.y, rgb.z, 1.0);
    }
}