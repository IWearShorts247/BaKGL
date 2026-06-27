#version 330 core

in vec3 Position_screenspace;
in vec3 uvCoords;

// Ouput data
out vec4 color;

uniform int  colorMode;
uniform vec4 blockColor;
uniform sampler2DArray texture0;
// Crossfade companion ("classic" art) — bound to texture unit 1. When a sprite
// sheet has a parallel original-art array (uHasCrossfade == 1) this is sampled
// at the same uvCoords and blended with texture0 by uCrossfade.
// uCrossfade: 0.0 = texture0 (remastered/override), 1.0 = texture1 (classic original).
uniform sampler2DArray texture1;
uniform float uCrossfade;
uniform int   uHasCrossfade;

// colorMode
// 0 :: Use Texture
// 1 :: Use Solid Color
// 2 :: Use mix of texture and solid color (TintColor)
// 3 :: Use solid color with texture alpha (ReplaceColor)

void main()
{
    vec4 textureSample = texture(texture0, uvCoords);
    if (uHasCrossfade == 1 && uCrossfade > 0.0)
    {
        vec4 classicSample = texture(texture1, uvCoords);
        textureSample = mix(textureSample, classicSample, uCrossfade);
    }
    vec3 textureColor  = textureSample.xyz;
    vec3 blockColorB   = blockColor.xyz;
    if (colorMode == 1) // block mode
        color = blockColor;
    else if (colorMode == 2) // tint mode
        color = vec4(
            mix(blockColorB, textureColor, .5),
            textureSample.a);
    else if (colorMode == 3) // replace mode
        color = vec4(
            blockColorB,
            textureSample.a);
    else
        color = vec4(textureColor, textureSample.a);
}
