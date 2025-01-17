// Basic diffuse Shader
#type vertex

#version 450 core
	
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_normal;


layout(std140, binding = 0) uniform camera
{
	mat4 u_ViewProjection;
};
uniform mat4 u_trans;

out vec3 v_pos;
out vec2 v_uv;
out vec3 v_normal;

void main(){
	v_normal = mat3(transpose(inverse(u_trans)))*a_normal;
	v_pos = vec3(u_trans*vec4(a_pos,1.0));
	v_uv = a_uv;
	gl_Position = u_ViewProjection * u_trans *vec4(a_pos,1.0);
}

#type fragment
#version 450 core
layout(location = 0) out vec4 fragColor;	

void main(){	
	fragColor = vec4(vec3(0.8,0.6,0.1),1.0);
}

