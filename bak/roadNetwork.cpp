#include "bak/roadNetwork.hpp"

#include "bak/worldFactory.hpp"

#include "com/logger.hpp"

#include "graphics/glm.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace BAK {

namespace {

float Dist(glm::vec2 a, glm::vec2 b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// minimal union-find
struct UnionFind
{
    std::vector<int> mParent;
    explicit UnionFind(std::size_t n) : mParent(n) { std::iota(mParent.begin(), mParent.end(), 0); }
    int Find(int a) { while (mParent[a] != a) { mParent[a] = mParent[mParent[a]]; a = mParent[a]; } return a; }
    void Union(int a, int b) { mParent[Find(a)] = Find(b); }
};

// Road faces are color index 1 on terrain ("t0") meshes.
constexpr std::uint8_t sRoadColorIndex = 1;

}

RoadNetwork::RoadNetwork(const WorldTileStore& worldTiles)
{
    const auto& logger = Logging::LogState::GetLogger("RoadNetwork");

    // 1. Extract road-face centroids (world x,z) from every tile's terrain mesh.
    std::vector<glm::vec2> raw;
    for (const auto& world : worldTiles.GetTiles())
    {
        for (const auto& inst : world.GetItems())
        {
            const auto& zi = inst.GetZoneItem();
            if (zi.GetName().substr(0, 2) != "t0") continue;

            const auto& verts = zi.GetVertices();
            const auto& faces = zi.GetFaces();
            const auto& colors = zi.GetColors();
            const auto loc = inst.GetLocation();
            const float scale = zi.GetScale();

            for (std::size_t f = 0; f < faces.size() && f < colors.size(); f++)
            {
                if (colors[f] != sRoadColorIndex) continue;
                glm::vec3 centroid{0};
                for (auto idx : faces[f]) centroid += glm::cast<float>(verts[idx]);
                centroid = centroid / static_cast<float>(faces[f].size()) * scale;
                raw.emplace_back(loc.x + centroid.x, loc.z + centroid.z);
            }
        }
    }

    if (raw.size() < 2)
    {
        logger.Debug() << "No road network (only " << raw.size() << " road faces)\n";
        return;
    }

    // 2. Median nearest-neighbour distance → clustering/edge thresholds.
    std::vector<float> nn;
    nn.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); i++)
    {
        float best = std::numeric_limits<float>::max();
        for (std::size_t j = 0; j < raw.size(); j++)
            if (i != j) best = std::min(best, Dist(raw[i], raw[j]));
        nn.push_back(best);
    }
    auto sorted = nn;
    std::sort(sorted.begin(), sorted.end());
    const float median = sorted[sorted.size() / 2];
    const float eps = median * 0.6f; // merge faces this close into one node
    const float cap = median * 3.0f; // candidate-edge distance cap (RNG prunes the rest)

    // 3. Merge nearby faces into nodes (union-find by proximity).
    UnionFind uf(raw.size());
    for (std::size_t i = 0; i < raw.size(); i++)
        for (std::size_t j = i + 1; j < raw.size(); j++)
            if (Dist(raw[i], raw[j]) < eps) uf.Union(static_cast<int>(i), static_cast<int>(j));

    std::vector<int> rep(raw.size(), -1);
    std::vector<glm::vec2> sums;
    std::vector<int> counts;
    for (std::size_t i = 0; i < raw.size(); i++)
    {
        const int r = uf.Find(static_cast<int>(i));
        if (rep[r] == -1) { rep[r] = static_cast<int>(sums.size()); sums.emplace_back(0.0f); counts.push_back(0); }
        sums[rep[r]] += raw[i];
        counts[rep[r]]++;
    }
    mNodes.resize(sums.size());
    for (std::size_t i = 0; i < sums.size(); i++)
        mNodes[i].mPos = sums[i] / static_cast<float>(counts[i]);

    // 4. Relative Neighborhood Graph edges (within cap): edge(i,j) iff no node k is
    //    closer to both i and j than they are to each other.
    for (std::size_t i = 0; i < mNodes.size(); i++)
    {
        for (std::size_t j = i + 1; j < mNodes.size(); j++)
        {
            const float d = Dist(mNodes[i].mPos, mNodes[j].mPos);
            if (d >= cap) continue;
            bool blocked = false;
            for (std::size_t k = 0; k < mNodes.size() && !blocked; k++)
                if (k != i && k != j
                    && Dist(mNodes[i].mPos, mNodes[k].mPos) < d
                    && Dist(mNodes[j].mPos, mNodes[k].mPos) < d)
                    blocked = true;
            if (!blocked)
            {
                mNodes[i].mNeighbours.push_back(static_cast<unsigned>(j));
                mNodes[j].mNeighbours.push_back(static_cast<unsigned>(i));
            }
        }
    }

    unsigned junctions = 0, deadEnds = 0;
    for (const auto& n : mNodes)
    {
        if (n.mNeighbours.size() >= 3) junctions++;
        if (n.mNeighbours.size() == 1) deadEnds++;
    }
    logger.Info() << "Built road network: " << mNodes.size() << " nodes, "
        << junctions << " junctions, " << deadEnds << " dead-ends\n";
}

std::optional<unsigned> RoadNetwork::FindNearestNode(glm::vec2 pos, float maxDistance) const
{
    std::optional<unsigned> best;
    float bestDist = maxDistance;
    for (std::size_t i = 0; i < mNodes.size(); i++)
    {
        const float d = Dist(mNodes[i].mPos, pos);
        if (d < bestDist) { bestDist = d; best = static_cast<unsigned>(i); }
    }
    return best;
}

}
