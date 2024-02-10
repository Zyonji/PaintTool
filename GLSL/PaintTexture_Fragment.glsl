uniform sampler2D Image;

smooth in vec2 FragUV;

out vec4 FragmentColor;

void main(void)
{
	float Checkered = 0.25 + 0.5 * mod(floor(gl_FragCoord.x / 8) + floor(gl_FragCoord.y / 8), 2);
	vec3 Background = vec3(Checkered);
	vec4 Color = texture(Image, FragUV);
    
	FragmentColor = vec4(mix(Background, Color.rgb, Color.a), 1.0);
    //FragmentColor = vec4(Color.rgb, 1.0);
}