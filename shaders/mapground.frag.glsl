#version 330 core

// Flat-shaded faceted ground. The face normal is recovered from screen-space derivatives of
// the world position, so each triangle catches the directional light differently and we get
// the original's faceted look without needing per-vertex normals.

in vec3 worldPos;

out vec4 fragColor;

uniform vec3 lightDir;   // scene light direction (points away from the light)
uniform vec3 baseColor;

void main()
{
    vec3 n = normalize(cross(dFdx(worldPos), dFdy(worldPos)));
    float diffuse = max(dot(n, normalize(-lightDir)), 0.0);
    float shade = 0.55 + 0.45 * diffuse;
    fragColor = vec4(baseColor * shade, 1.0);
}
