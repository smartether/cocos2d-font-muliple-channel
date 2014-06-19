

#ifdef GL_ES								
precision mediump float;						
#endif										
												
varying mat4 v_fc_tc_fc;
uniform sampler2D CC_Texture0;				
											
void main()									
{	    
		vec4 texColor =  texture2D(CC_Texture0, vec2(v_fc_tc_fc[2].x,v_fc_tc_fc[2].y));
		texColor.a = dot(texColor, vec4(v_fc_tc_fc[1]));	
		texColor = texColor.a>0.5?(texColor.a*vec4(2,2,2,0)+vec4(-1,-1,-1,1)):texColor.a*vec4(0,0,0,2);
		//gl_FragColor = vec4(v_fc_tc_fc[1].x,v_fc_tc_fc[1].y,v_fc_tc_fc[1].z,1);
		//gl_FragColor =texColor * vec4(v_fc_tc_fc[0].x,v_fc_tc_fc[0].y,v_fc_tc_fc[0].z,v_fc_tc_fc[0].w);
		gl_FragColor = mix(v_fc_tc_fc[0],texColor,texColor.a);
}											

