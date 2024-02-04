in vec4 VertP;
in vec2 VertUV;

smooth out vec2 FragUV;

void main(void)
{
	gl_Position = VertP;
	FragUV = VertUV;
}