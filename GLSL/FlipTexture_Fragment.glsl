uniform ivec2 Offset;
uniform ivec2 Stride;
uniform sampler2D Image;

out vec4 FragmentColor;

void main(void)
{
    ivec2 Target = Stride * ivec2(gl_FragCoord.xy) + Offset;
    FragmentColor = texelFetch(Image, Target, 0);
}