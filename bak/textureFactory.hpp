#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Graphics {
class TextureStore;
class Texture;
}

namespace BAK {

class Image;
class Palette;

// Load a PNG at its native resolution; targetWidth/Height become the texture's
// layout/UV target (the on-screen size the hi-res image fills). Exposed so the
// combat-sprite override path can substitute upscaled frames.
Graphics::Texture PNGToTexture(
    std::string path,
    unsigned targetWidth,
    unsigned targetHeight);

class TextureFactory
{
public:
    static Graphics::TextureStore MakeTextureStore(
        std::string_view bmx,
        std::string_view pal);

    static void AddToTextureStore(
        Graphics::TextureStore&,
        std::string_view bmx,
        std::string_view pal);

    // Crossfade-aware variant. Adds, at the SAME index in both stores:
    //  - primary:   the override (upscaled) image if one exists, else the original.
    //  - companion: ALWAYS the original art, point-resized to the primary entry's
    //               pixel dimensions so a single set of UVs addresses both arrays.
    // When no override exists the two entries are identical (crossfade = no-op).
    // Used by the classic/remastered crossfade toggle; single-image BMX only
    // (portraits) — multi-image falls back to mirroring the original into both.
    static void AddToTextureStoreCrossfade(
        Graphics::TextureStore& primary,
        Graphics::TextureStore& companion,
        std::string_view bmx,
        std::string_view pal);

    static void AddScreenToTextureStore(
        Graphics::TextureStore&,
        std::string_view scx,
        std::string_view pal);

    static void AddTerrainToTextureStore(
        Graphics::TextureStore&,
        const Image& terrain,
        const Palette&);

    static void AddToTextureStore(
        Graphics::TextureStore& store,
        const Image& image,
        const Palette& palette);

    static void AddToTextureStore(
        Graphics::TextureStore& store,
        const std::vector<Image>& images,
        const Palette& palette);
};

}
