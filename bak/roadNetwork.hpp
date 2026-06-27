#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace BAK {

class WorldTileStore;

// A navigable graph of the roads in a zone, derived from the road-textured faces
// of the terrain meshes. Nodes are road points (world x,z, GL coords); edges connect
// neighbouring road points (Relative Neighborhood Graph) so a degree-2 node lies along
// a road and degree>=3 marks a junction.
class RoadNetwork
{
public:
    struct Node
    {
        glm::vec2 mPos; // world (x, z) in GL coordinates
        std::vector<unsigned> mNeighbours;
    };

    RoadNetwork() = default;
    explicit RoadNetwork(const WorldTileStore& worldTiles);

    bool Empty() const { return mNodes.empty(); }
    const std::vector<Node>& GetNodes() const { return mNodes; }

    // Nearest road node to a world position, within maxDistance (GL units). nullopt if none.
    std::optional<unsigned> FindNearestNode(glm::vec2 pos, float maxDistance) const;

    unsigned GetDegree(unsigned node) const { return static_cast<unsigned>(mNodes[node].mNeighbours.size()); }
    bool IsJunction(unsigned node) const { return GetDegree(node) >= 3; }
    bool IsDeadEnd(unsigned node) const { return GetDegree(node) == 1; }

private:
    std::vector<Node> mNodes;
};

}
