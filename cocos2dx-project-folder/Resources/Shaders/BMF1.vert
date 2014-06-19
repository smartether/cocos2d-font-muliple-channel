													
attribute vec4 a_position;							
attribute vec2 a_texCoord;							
attribute vec4 a_color;								
													
#ifdef GL_ES										
varying lowp mat4 v_fc_tc_fc;		
#else											
varying mat4 v_fc_tc_fc;					
#endif
#ifdef GL_ES								
precision lowp float;						
#endif											
				
void main()											
{
	const mat4 mchan = mat4(vec4(0,0,1,0),vec4(0,1,0,0),vec4(1,0,0,0),vec4(0,0,0,1));	
	//float channel = a_position.z;						
    gl_Position = CC_MVPMatrix * vec4(a_position.x,a_position.y,0,a_position.w);
	v_fc_tc_fc = mat4(a_color, vec4(mchan[int(a_position.z * 0.28)]), vec4(a_texCoord.x,a_texCoord.y,0,0), vec4(mchan[0]));
}													
