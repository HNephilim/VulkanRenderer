#version 450

layout(location = 0) in vec3 inColour;

layout(location = 0) out vec4 outColour; 	// Final output colour (must also have location

void main() {
	outColour = vec4(inColour, 1.0);
}