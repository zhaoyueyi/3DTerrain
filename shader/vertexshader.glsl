#version 330 core

layout(location = 0) in vec3 vertexPosition_modelspace;  // glEnableVertexAttribArray()
layout(location = 1) in vec3 vertexColor;
// layout(location = 2) in vec2 vertexUV;

uniform mat4 MVP;

out vec3 fragmentColor;
// out vec2 UV;

void main(){
	gl_Position = MVP * vec4(vertexPosition_modelspace, 1);
	// fragmentColor = vertexColor;
	//fragmentColor.x = vertexPosition_modelspace[1]/20;
	//fragmentColor.y = 0;
	//fragmentColor.z = 255 - fragmentColor.x;
	fragmentColor = vec3(vertexPosition_modelspace[1]*300/255, 0, 1 - vertexPosition_modelspace[1]*300/255);
}