#define M_PI 3.1415926535897932384626433832795

uniform sampler2D Image;
uniform int ChannelCount;

out vec4 FragmentColor;

void main(void)
{
    ivec2 Target = ivec2(gl_FragCoord.xy) / 8 * 8;
    
    vec4 Color = vec4(0.0);
    vec4 OffsetX = mod(vec4(gl_FragCoord.x), 8.0);
    vec4 OffsetY = mod(vec4(gl_FragCoord.y), 8.0);
    vec4 ScalarX, ScalarY;
    
    for(int y = 0; y < 8; y++)
    {
        for(int x = 0; x < 8; x++)
        {
            vec4 Texel;
            Texel = texelFetch(Image, Target + ivec2(x, y), 0);
            ScalarX = sqrt((clamp(x, 0.0, 1.0) + 1.0) / 8.0) * cos(x * M_PI * OffsetX / 8.0);
            ScalarY = sqrt((clamp(y, 0.0, 1.0) + 1.0) / 8.0) * cos(y * M_PI * OffsetY / 8.0);
            Color += ScalarX * ScalarY * Texel;
        }
    }
    
    if(ChannelCount < 4)
        Color.a = 1.0;
    
    FragmentColor = Color;
}