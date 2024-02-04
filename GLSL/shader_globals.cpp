static char *PaintChecker_FragmentCode = R"glsl(
out vec4 FragmentColor;

void main(void)
{
	float Checkered = 0.25 + 0.5 * mod(floor(gl_FragCoord.x / 8) + floor(gl_FragCoord.y / 8), 2);
	
    FragmentColor = vec4(vec3(Checkered), 1.0);
}
)glsl";

static char *PaintChecker_VertexCode = R"glsl(
in vec4 VertP;

void main(void)
{
	gl_Position = VertP;
}
)glsl";

static char *PaintTexture_FragmentCode = R"glsl(
uniform sampler2D Image;

smooth in vec2 FragUV;

out vec4 FragmentColor;

void main(void)
{
	float Checkered = 0.25 + 0.5 * mod(floor(gl_FragCoord.x / 8) + floor(gl_FragCoord.y / 8), 2);
	vec3 Background = vec3(Checkered);
	vec4 Color = texture(Image, FragUV);
    
	FragmentColor = vec4(mix(Background, Color.rgb, Color.a), 1.0);
}
)glsl";

static char *PaintTexture_VertexCode = R"glsl(
in vec4 VertP;
in vec2 VertUV;

smooth out vec2 FragUV;

void main(void)
{
	gl_Position = VertP;
	FragUV = VertUV;
}
)glsl";

