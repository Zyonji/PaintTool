uniform sampler2D Image;
uniform ivec4 SampleX;
uniform ivec4 SampleY;

out vec4 FragmentColor;

void main(void)
{
    vec2 PixelCenter = gl_FragCoord.xy;
    
    vec4 OffsetX = vec4(PixelCenter.x) / SampleX - vec4(0.5);
    vec4 OffsetY = vec4(PixelCenter.y) / SampleY - vec4(0.5);
    
    vec2 TargetR = vec2(OffsetX.r, OffsetY.r);
    vec2 TargetG = vec2(OffsetX.g, OffsetY.g);
    vec2 TargetB = vec2(OffsetX.b, OffsetY.b);
    vec2 TargetA = vec2(OffsetX.a, OffsetY.a);
    
    OffsetX = fract(OffsetX);
    OffsetY = fract(OffsetY);
    
    vec4 Texel00;
    Texel00.r = texelFetch(Image, ivec2(TargetR), 0).r;
    Texel00.g = texelFetch(Image, ivec2(TargetG), 0).g;
    Texel00.b = texelFetch(Image, ivec2(TargetB), 0).b;
    Texel00.a = texelFetch(Image, ivec2(TargetA), 0).a;
    
    vec4 Texel01;
    Texel01.r = texelFetch(Image, ivec2(TargetR + vec2(0, 1)), 0).r;
    Texel01.g = texelFetch(Image, ivec2(TargetG + vec2(0, 1)), 0).g;
    Texel01.b = texelFetch(Image, ivec2(TargetB + vec2(0, 1)), 0).b;
    Texel01.a = texelFetch(Image, ivec2(TargetA + vec2(0, 1)), 0).a;
    
    vec4 Texel10;
    Texel10.r = texelFetch(Image, ivec2(TargetR + vec2(1, 0)), 0).r;
    Texel10.g = texelFetch(Image, ivec2(TargetG + vec2(1, 0)), 0).g;
    Texel10.b = texelFetch(Image, ivec2(TargetB + vec2(1, 0)), 0).b;
    Texel10.a = texelFetch(Image, ivec2(TargetA + vec2(1, 0)), 0).a;
    
    vec4 Texel11;
    Texel11.r = texelFetch(Image, ivec2(TargetR + vec2(1, 1)), 0).r;
    Texel11.g = texelFetch(Image, ivec2(TargetG + vec2(1, 1)), 0).g;
    Texel11.b = texelFetch(Image, ivec2(TargetB + vec2(1, 1)), 0).b;
    Texel11.a = texelFetch(Image, ivec2(TargetA + vec2(1, 1)), 0).a;
    
    vec4 Color = mix(mix(Texel00, Texel01, OffsetY), mix(Texel10, Texel11, OffsetY), OffsetX);
    
    //Color.y = Texel00.y;
    //Color.z = Texel11.z;
    
    if(SampleX.r * SampleY.r == 0)
        Color.r = 0.0;
    if(SampleX.g * SampleY.g == 0)
        Color.g = 0.0;
    if(SampleX.b * SampleY.b == 0)
        Color.b = 0.0;
    if(SampleX.a * SampleY.a == 0)
        Color.a = 1.0;
    
    FragmentColor = Color;
}