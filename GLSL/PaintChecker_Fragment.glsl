out vec4 FragmentColor;

void main(void)
{
	float Checkered = 0.25 + 0.5 * mod(floor(gl_FragCoord.x / 8) + floor(gl_FragCoord.y / 8), 2);
	
    FragmentColor = vec4(vec3(Checkered), 1.0);
}