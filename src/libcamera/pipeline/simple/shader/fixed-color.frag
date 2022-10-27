/* SPDX-License-Identifier: BSD-2-Clause */

/* Fixed color pattern generator */

#ifdef GL_ES
precision mediump float;
#endif

varying vec2 textureOut;

uniform sampler2D tex_y;

void main(void)
{
	gl_FragColor = vec4(0.2,0.5,0.8, 1.0);
}
