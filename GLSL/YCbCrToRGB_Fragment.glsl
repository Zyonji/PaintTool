uniform sampler2D Image;

out vec4 FragmentColor;

void main(void)
{
    vec4 Texel = texelFetch(Image, ivec2(gl_FragCoord.xy), 0);
    
    vec3 Color;
    Color.r = Texel.z * (2.0 - 2.0 * 0.299) + Texel.x;
    Color.b = Texel.y * (2.0 - 2.0 * 0.114) + Texel.x;
    Color.g = (Texel.x - 0.114 * Color.b - 0.299 * Color.r) / 0.587;
    Color = (Color + 128.0) / 255.0;
    
    FragmentColor = vec4(Color, Texel.a);
}