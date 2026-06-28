#version 330 core

// Local-map void filler: a faceted ground layer drawn under the live zone so gaps beyond
// the zone's tiles read as big shaded terrain polygons (imitating the original game) rather
// than flat colour. Positions are a flat grid (normalised world units); per-vertex height is
// a world-stable hash so the facets don't swim as the party moves (the grid is snapped to a
// cell boundary on the CPU side).

layout(location = 0) in vec2 gridPos; // local grid x,z

uniform mat4 VP;          // projection * view (map camera)
uniform vec2 partyOffset; // grid-snapped party x,z (normalised world)
uniform float cell;
uniform float amp;
uniform float baseY;

out vec3 worldPos;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main()
{
    vec2 xz = gridPos + partyOffset;
    float h = baseY + (hash(xz / cell) - 0.5) * amp;
    worldPos = vec3(xz.x, h, xz.y);
    gl_Position = VP * vec4(worldPos, 1.0);
}
