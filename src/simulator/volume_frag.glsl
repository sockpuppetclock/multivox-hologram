#version 310 es

precision mediump float;
precision mediump isampler3D;

uniform isampler3D u_volume;

in vec3 v_texcoord;
in vec2 v_dotcoord;

out vec4 ql_FragColor;

float dot2(vec2 v) {return dot(v, v);}

void main() {
    float rsq = dot2(fract(v_dotcoord)-vec2(0.5,0.5));
    float lum = max(0.0, 0.5 - rsq * 4.0);

    float dm = v_dotcoord.x + v_dotcoord.y;
    float dd = min(1.0, 0.125 / dot2(vec2(dFdx(dm), dFdy(dm))));
    lum = mix(0.33, lum, dd);

    int pix = texture(u_volume, v_texcoord).r;

    /*vec3 colour = vec3(
        float((pix&0xe0)>>5) / 7.0,
        float((pix&0x1c)>>2) / 7.0,
        float((pix&0x03)<<1) / 7.0
    );*/
    vec3 colour = vec3(
        float((pix&0xc0)>>6) / 3.0,
        float((pix&0x18)>>3) / 3.0,
        float((pix&0x03)   ) / 3.0
    );

    ql_FragColor.rgb = colour * lum;
    ql_FragColor.a = 1.0;
}
