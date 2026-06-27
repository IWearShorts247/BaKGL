#include "bak/textureFactory.hpp"

#include "bak/image.hpp"
#include "bak/imageStore.hpp"
#include "bak/palette.hpp"
#include "bak/screen.hpp"
#include "bak/fileBufferFactory.hpp"

#include "com/logger.hpp"
#include "com/png.hpp"
#include "com/path.hpp"
#include "com/string.hpp"

#include "graphics/texture.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <random>

namespace BAK {

Graphics::Texture ImageToTexture(const Image& image, const Palette& palette)
{
    auto texture = Graphics::Texture::TextureType{};
    const auto imageSize = image.GetWidth() * image.GetHeight();
    texture.reserve(imageSize);

    auto* pixels = image.GetPixels();

    for (unsigned i = 0; i < imageSize; i++)
    {
        texture.push_back(palette.GetColor(pixels[i]));
    }

    auto tex = Graphics::Texture{
        texture,
        static_cast<unsigned>(image.GetWidth()),
        static_cast<unsigned>(image.GetHeight()),
        static_cast<unsigned>(image.GetWidth()),
        static_cast<unsigned>(image.GetHeight()) };

    // For OpenGL
    tex.Invert();

    return tex;
}

// Cap on the resolution an override PNG occupies on the GPU. Upscales can be
// ~2200px, but a portrait only ever displays in a small box AND every layer of
// the sprite-array is padded to the sheet's max dimension — so one oversized
// override balloons VRAM for the whole sheet (doubly so with a crossfade
// companion) and OOMs weaker GPUs. 1280 is well above the on-screen display
// size, so the downscale is visually lossless where it matters.
constexpr unsigned sMaxOverrideDim = 1280;

Graphics::Texture PNGToTexture(std::string path, unsigned targetWidth, unsigned targetHeight)
{
    const auto image = LoadPNG(path.c_str());
    const unsigned width = image.mWidth;
    const unsigned height = image.mHeight;

    unsigned outW = width;
    unsigned outH = height;
    if (std::max(width, height) > sMaxOverrideDim)
    {
        const double scale = static_cast<double>(sMaxOverrideDim) / std::max(width, height);
        outW = std::max(1u, static_cast<unsigned>(width * scale));
        outH = std::max(1u, static_cast<unsigned>(height * scale));
    }

    const auto Get = [&](unsigned x, unsigned y){
        return image.mPixels[y * width + x];
    };
    const auto F = [](auto v){ return static_cast<float>(v) / 255.f; };

    auto texture = Graphics::Texture::TextureType{};
    texture.reserve(static_cast<std::size_t>(outW) * outH);

    // Emit bottom-to-top (OpenGL origin), area-averaging each source block into
    // one output texel. When outW/outH == width/height the block is 1x1, i.e. an
    // exact copy + vertical flip (the original behaviour).
    for (int oy = static_cast<int>(outH) - 1; oy >= 0; oy--)
    {
        const unsigned y0 = (static_cast<unsigned>(oy) * height) / outH;
        const unsigned y1 = std::max(y0 + 1, ((static_cast<unsigned>(oy) + 1) * height) / outH);
        for (unsigned ox = 0; ox < outW; ox++)
        {
            const unsigned x0 = (ox * width) / outW;
            const unsigned x1 = std::max(x0 + 1, ((ox + 1) * width) / outW);
            glm::vec4 acc{0};
            unsigned n = 0;
            for (unsigned sy = y0; sy < y1; sy++)
                for (unsigned sx = x0; sx < x1; sx++)
                {
                    auto c = Get(sx, sy);
                    acc += glm::vec4{F(c.r), F(c.g), F(c.b), F(c.a)};
                    ++n;
                }
            texture.push_back(n ? acc / static_cast<float>(n) : acc);
        }
    }
    return Graphics::Texture{texture, outW, outH, targetWidth, targetHeight};
}

Graphics::TextureStore TextureFactory::MakeTextureStore(
    std::string_view bmx,
    std::string_view pal)
{
    auto store = Graphics::TextureStore{};
    AddToTextureStore(store, bmx, pal);
    return store;
}

void TextureFactory::AddToTextureStore(
    Graphics::TextureStore& store,
    std::string_view bmx,
    std::string_view pal)
{
    const auto palette = Palette{std::string{pal}};

    auto fb = FileBufferFactory::Get()
        .CreateDataBuffer(std::string{bmx});
    const auto images = LoadImages(fb);

    auto baseName = SplitString(".", std::string(bmx))[0];
    auto substitute = images.size() > 1
        ? Paths::Get().GetModDirectoryPath() / (baseName + ".BMX")
        : Paths::Get().GetModDirectoryPath() / (baseName + ".PNG");
    if (std::filesystem::exists(substitute) && images.size() == 1)
    {
        auto tex = PNGToTexture(substitute.string(), images.back().GetWidth(), images.back().GetHeight());
        Logging::LogDebug(__FUNCTION__) << "Found substitute BMX: " << substitute
          << " Dims: (" << tex.GetWidth() << ", " << tex.GetHeight() << ") TargetDims: ("
          << tex.GetTargetWidth() << ", " << tex.GetTargetHeight() << ")\n";
        store.AddTexture(tex);
    }
    else if (std::filesystem::exists(substitute))
    {
        for (unsigned i = 0; i < images.size(); i++)
        {
            std::stringstream name{};
            name << i << ".PNG";
            auto path = substitute / name.str();
            if (std::filesystem::exists(path))
            {
                auto tex = PNGToTexture(path.string(), images[i].GetWidth(), images[i].GetHeight());
                Logging::LogDebug(__FUNCTION__) << "Found substitute BMX: " << path 
                  << " Dims: (" << tex.GetWidth() << ", " << tex.GetHeight() << ") TargetDims: ("
                  << tex.GetTargetWidth() << ", " << tex.GetTargetHeight() << ")\n";
                store.AddTexture(tex);
            }
            else
            {
                AddToTextureStore(store, images[i], palette);
            }
        }
    }
    else
    {
        AddToTextureStore(store, images, palette);
    }
}

namespace {
// Nearest-neighbour resize, preserving orientation. Used to blow the small
// original art up to the override's pixel grid so both share one UV mapping.
Graphics::Texture ResizeNearest(
    const Graphics::Texture& src,
    unsigned newWidth,
    unsigned newHeight,
    unsigned targetWidth,
    unsigned targetHeight)
{
    auto dst = Graphics::Texture{newWidth, newHeight, targetWidth, targetHeight};
    const auto srcW = src.GetWidth();
    const auto srcH = src.GetHeight();
    for (unsigned y = 0; y < newHeight; y++)
        for (unsigned x = 0; x < newWidth; x++)
            dst.SetPixel(x, y, src.GetPixel(
                (x * srcW) / newWidth,
                (y * srcH) / newHeight));
    return dst;
}
}

void TextureFactory::AddToTextureStoreCrossfade(
    Graphics::TextureStore& primary,
    Graphics::TextureStore& companion,
    std::string_view bmx,
    std::string_view pal)
{
    const auto palette = Palette{std::string{pal}};

    auto fb = FileBufferFactory::Get()
        .CreateDataBuffer(std::string{bmx});
    const auto images = LoadImages(fb);

    auto baseName = SplitString(".", std::string(bmx))[0];
    const auto substitute = Paths::Get().GetModDirectoryPath() / (baseName + ".PNG");

    if (images.size() == 1 && std::filesystem::exists(substitute))
    {
        const auto& image = images.back();
        auto original = ImageToTexture(image, palette);
        auto upscaled = PNGToTexture(
            substitute.string(), image.GetWidth(), image.GetHeight());
        // Resize the classic art up to the override's pixel grid so the UVs match.
        auto classic = ResizeNearest(
            original,
            upscaled.GetWidth(), upscaled.GetHeight(),
            upscaled.GetTargetWidth(), upscaled.GetTargetHeight());
        primary.AddTexture(upscaled);
        companion.AddTexture(classic);
    }
    else
    {
        // No override (or multi-image): both arrays get the identical original.
        const auto before = primary.size();
        AddToTextureStore(primary, images, palette);
        for (auto i = before; i < primary.size(); i++)
            companion.AddTexture(primary.GetTexture(i));
    }
}

void TextureFactory::AddScreenToTextureStore(
    Graphics::TextureStore& store,
    std::string_view scx,
    std::string_view pal)
{
    auto baseName = SplitString(".", std::string(scx))[0];
    auto substitute = Paths::Get().GetModDirectoryPath() / (baseName + ".PNG");

    if (std::filesystem::exists(substitute))
    {
        auto fb = FileBufferFactory::Get()
            .CreateDataBuffer(std::string{scx});
        auto target = LoadScreenResource(fb);
        Logging::LogDebug(__FUNCTION__) << "Found substitute SCX: " << substitute << "\n";
        store.AddTexture(PNGToTexture(substitute.string(), target.GetWidth(), target.GetHeight()));
    }
    else
    {
        const auto palette = Palette{std::string{pal}};
        auto fb = FileBufferFactory::Get()
            .CreateDataBuffer(std::string{scx});
        AddToTextureStore(store, LoadScreenResource(fb), palette);
    }
}

void TextureFactory::AddTerrainToTextureStore(
    Graphics::TextureStore& store,
    const Image& terrain,
    const Palette& palette)
{
    auto* pixels = terrain.GetPixels();
    auto width = terrain.GetWidth();

    auto startOff = 0;
    // FIXME: Can I find these in the data files somewhere?
    for (auto offset : {70, 20, 20, 32, 20, 27, 6, 5})
    {
        auto image = Graphics::Texture::TextureType{};
        const auto imageStart = startOff * width;
        const auto imageEnd = (startOff + offset) * width;
        image.reserve(imageEnd - imageStart);
        for (unsigned i = imageStart; i < imageEnd; i++)
        {
            const auto& color = palette.GetColor(pixels[i]);
            image.push_back(color);
        }
        if (offset == 70)
        {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(image.begin(), image.end(), g);
        }

        startOff += offset;
        
        store.AddTexture(
            Graphics::Texture{
                image,
                static_cast<unsigned>(width),
                static_cast<unsigned>(offset),
                static_cast<unsigned>(width),
                static_cast<unsigned>(offset)});
    }
}

void TextureFactory::AddToTextureStore(
    Graphics::TextureStore& store,
    const Image& image,
    const Palette& palette)
{
    store.AddTexture(ImageToTexture(image, palette));
}

void TextureFactory::AddToTextureStore(
    Graphics::TextureStore& store,
    const std::vector<Image>& images,
    const Palette& palette)
{
    for (const auto& image : images)
        AddToTextureStore(store, image, palette);
}

} // namespace BAK {
