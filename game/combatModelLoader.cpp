#include "game/combatModelLoader.hpp"

#include "bak/combat/combatModel.hpp"
#include "bak/fileBufferFactory.hpp"
#include "bak/image.hpp"
#include "bak/imageStore.hpp"
#include "bak/model.hpp"
#include "bak/monster.hpp"
#include "bak/palette.hpp"
#include "bak/resourceNames.hpp"
#include "bak/textureFactory.hpp"
#include "bak/worldFactory.hpp"

#include "com/logger.hpp"
#include "com/ostream.hpp"
#include "com/path.hpp"
#include "com/string.hpp"

#include <filesystem>
#include <iomanip>

#include "graphics/meshObject.hpp"
#include "graphics/texture.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Game {

CombatModelLoader::CombatModelLoader()
{
    const auto& logger = Logging::Logger("CombatModelLoader");
    auto tblBuf = BAK::FileBufferFactory::Get().CreateDataBuffer(sCombatModels);
    auto models = BAK::LoadTBL(tblBuf);
    for (unsigned i = 0; i < models.size(); i++)
    {
        if (models[i].mEntityType == 0 && models[i].mSprite > 0)
        {
            logger.Debug() << "Loaded Combatant Model #" << i << " " << models[i].mName << "\n";
            mCombatModels.emplace_back(BAK::CombatModel{models[i]});
            LoadMonsterSprites(BAK::MonsterIndex{i});
        }
        else
        {
            mCombatModels.emplace_back(std::nullopt);
            mCombatModelDatas.emplace_back(std::nullopt);
        }
    }
}

void CombatModelLoader::LoadMonsterSprites(BAK::MonsterIndex m)
{
    const auto& logger = Logging::LogState::GetLogger(__FUNCTION__);
    logger.Spam() << "Loading monster: " << m << "\n";
    const auto& mnames = BAK::MonsterNames::Get();
    auto textureStore = Graphics::TextureStore{};
    auto monster = mnames.GetMonster(m);
    logger.Spam() << "Monster Sprites: " << monster << "\n";
    const auto prefix = ToUpper(monster.mPrefix);
    if (prefix.empty())
    {
        return;
    }

    auto pal = BAK::Palette{BAK::ZoneLabel{1}.GetPalette()};
    if (monster.mColorSwap <= 9)
    {
        auto ss = std::stringstream{};
        ss << "CS";
        ss << +monster.mColorSwap << ".DAT";
        const auto cs = BAK::ColorSwap{ss.str()};
        pal = BAK::Palette{pal, cs};
    }

    // Hi-res override key suffix: the colour variant baked into the PNGs. Combat
    // sprites are coloured (Zone1 + ColorSwap) per monster, so the same sprite
    // sheet has different art per colorswap — the override is keyed by both.
    const std::string csTag = monster.mColorSwap <= 9
        ? "cs" + std::to_string(+monster.mColorSwap)
        : "base";

    auto LoadImages = [&](auto suffix)
    {
        std::stringstream sheetSs{};
        sheetSs << prefix << +suffix;                 // e.g. "GNT1"
        const auto sheet = sheetSs.str();

        auto fb = BAK::FileBufferFactory::Get().CreateDataBuffer(sheet + ".BMX");
        const auto images = BAK::LoadImages(fb);

        // <ModDir>/combat/<SHEET>__<csTag>/<SHEET>__<csTag>_<NN>.PNG, one per frame.
        // Each PNG already has the final colour baked in, so it REPLACES the
        // palettised frame (no ColorSwap applied). Missing frames fall back to the
        // decoded BMX, so a partial override pack works. Frame count/order are
        // preserved, keeping the animation offset table valid.
        const auto base = sheet + "__" + csTag;
        const auto dir = Paths::Get().GetModDirectoryPath() / "combat" / base;
        const bool haveOverride = std::filesystem::exists(dir);
        if (haveOverride)
            logger.Debug() << "Using hi-res override for combat sheet: " << base
                << " (" << images.size() << " frames)\n";

        for (unsigned i = 0; i < images.size(); i++)
        {
            std::stringstream pngSs{};
            pngSs << base << "_" << std::setw(2) << std::setfill('0') << i << ".PNG";
            const auto path = dir / pngSs.str();
            if (haveOverride && std::filesystem::exists(path))
            {
                textureStore.AddTexture(BAK::PNGToTexture(
                    path.string(),
                    images[i].GetWidth(),
                    images[i].GetHeight()));
            }
            else
            {
                BAK::TextureFactory::AddToTextureStore(textureStore, images[i], pal);
            }
        }
    };

    std::vector<std::size_t> offsets{0};
    LoadImages(monster.mSuffix0);
    offsets.emplace_back(textureStore.size());
    LoadImages(monster.mSuffix1);
    offsets.emplace_back(textureStore.size());
    LoadImages(monster.mSuffix2);

    logger.Spam() << "Loaded all textures: " << textureStore.size()
        << " Offsets: " << offsets << "\n";

    assert(mCombatModels[m.mValue]);
    const auto& model = *mCombatModels[m.mValue];
    Graphics::MeshObjectStorage objects{};
    std::unordered_map<AnimationRequest, AnimationOffset> offsetMap{};
    std::vector<Graphics::MeshObjectStorage::OffsetAndLength> objectOffsets{};
    using enum BAK::Direction;
    std::stringstream ss{};
    for (const auto& animType : model.GetSupportedAnimations())
    {
        logger.Spam() << "AnimatioNType: " << BAK::ToString(animType) << "(" << +std::to_underlying(animType) << ")\n";
        for (const auto& direction : model.GetDirections(animType))
        {
            logger.Spam() << "Direction: " << BAK::ToString(direction) << "\n";
            const auto& animation = model.GetAnimation(animType, direction);
            offsetMap.emplace(
                AnimationRequest{animType, direction},
                AnimationOffset{objects.size(), animation.mImageIndices.size()});

            for (const auto& index : animation.mImageIndices)
            {
                assert(animation.mSpriteFileIndex < offsets.size());
                logger.Spam() << "SFI: " << +animation.mSpriteFileIndex << " offset: "
                    << offsets[animation.mSpriteFileIndex] << " index: " << +index << "\n";
                const auto fullOffset = offsets[animation.mSpriteFileIndex] + index;
                if (fullOffset >= textureStore.size())
                {
                    logger.Error() << "Image offset past loaded textures:" << fullOffset << " >= size: " << textureStore.size() << "\n";
                    continue;
                }
                auto zoneItem = BAK::ZoneItem(fullOffset, textureStore.GetTexture(fullOffset));
                ss.clear();
                ss << objects.size();
                auto offset = objects.AddObject(
                    ss.str(),
                    BAK::ZoneItemToMeshObject(zoneItem, textureStore));
                objectOffsets.emplace_back(offset);
            }
        }
    }

    auto renderData = Graphics::RenderData{};
    renderData.LoadData(objects, textureStore.GetTextures(), textureStore.GetMaxDim());
    mCombatModelDatas.emplace_back(
        std::make_optional(
            CombatModelData{
                std::move(renderData),
                std::move(offsetMap),
                std::move(objectOffsets)}));
    Logging::LogDebug(__FUNCTION__) <<"Loaded model: " << prefix << " at index: " << mCombatModelDatas.size() - 1 << "\n";
}

}

namespace std {

std::size_t hash<Game::AnimationRequest>::operator()(const Game::AnimationRequest& t) const noexcept
{
    return std::hash<std::size_t>{}(
        std::to_underlying(t.mAnimation) | (std::to_underlying(t.mDirection) << 8));
}

}
