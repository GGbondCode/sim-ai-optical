#ifndef OPTICAL_TYPES_H
#define OPTICAL_TYPES_H

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace ns3 {

static constexpr uint32_t OPTICAL_INVALID_NODE_ID = std::numeric_limits<uint32_t>::max ();

struct NodeState
{
  uint32_t node_id = OPTICAL_INVALID_NODE_ID;
  bool alive = true;
  std::vector<bool> gpu_alive;
  bool optical_port_available = true;
};

struct CircuitState
{
  uint32_t a = OPTICAL_INVALID_NODE_ID;
  uint32_t b = OPTICAL_INVALID_NODE_ID;
  bool active = false;
  bool backup = false;
  bool loopback = false;
  uint64_t setup_delay_ns = 0;
};

struct RingNeighbor
{
  uint32_t prev = OPTICAL_INVALID_NODE_ID;
  uint32_t next = OPTICAL_INVALID_NODE_ID;
};

inline bool
operator== (const RingNeighbor& lhs, const RingNeighbor& rhs)
{
  return lhs.prev == rhs.prev && lhs.next == rhs.next;
}

struct FaultEvent
{
  enum Type
  {
    NODE,
    GPU,
    LINK
  };

  Type type = NODE;
  uint32_t node_id = OPTICAL_INVALID_NODE_ID;
  uint32_t peer_id = OPTICAL_INVALID_NODE_ID;
  uint32_t gpu_id = OPTICAL_INVALID_NODE_ID;
  uint64_t time_ns = 0;
};

struct ClusterState
{
  std::vector<NodeState> nodes;
  std::vector<CircuitState> circuits;
  std::vector<RingNeighbor> ring;
};

struct ReconfigPlan
{
  std::vector<std::pair<uint32_t, uint32_t>> links_to_remove;
  std::vector<std::pair<uint32_t, uint32_t>> links_to_add;
  std::vector<RingNeighbor> new_ring;
  std::string reason;
};

inline std::pair<uint32_t, uint32_t>
MakeCanonicalLink (uint32_t a, uint32_t b)
{
  return (a < b) ? std::make_pair (a, b) : std::make_pair (b, a);
}

inline bool
IsValidNodeId (uint32_t nodeId)
{
  return nodeId != OPTICAL_INVALID_NODE_ID;
}

} // namespace ns3

#endif // OPTICAL_TYPES_H
