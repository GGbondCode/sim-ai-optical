#ifndef COMMUNICATION_TOPOLOGY_MANAGER_H
#define COMMUNICATION_TOPOLOGY_MANAGER_H

#include "optical-types.h"
#include "ns3/object.h"

#include <string>
#include <vector>

namespace ns3 {

class CommunicationTopologyManager : public Object
{
public:
  static TypeId GetTypeId (void);

  CommunicationTopologyManager ();
  ~CommunicationTopologyManager () override;

  void SetRingOrder (const std::vector<uint32_t>& ringOrder);
  void ApplyRing (const std::vector<RingNeighbor>& ring);

  std::vector<uint32_t> GetRingOrder () const;
  const std::vector<RingNeighbor>& GetRingNeighbors () const;
  RingNeighbor GetRingNeighbor (uint32_t nodeId) const;
  std::string DescribeRing () const;

  static std::vector<uint32_t> BuildRingOrder (const std::vector<RingNeighbor>& ring);
  static std::string DescribeRing (const std::vector<RingNeighbor>& ring);

private:
  static std::vector<RingNeighbor> BuildRingNeighbors (const std::vector<uint32_t>& ringOrder);

  std::vector<uint32_t> m_ringOrder;
  std::vector<RingNeighbor> m_ring;
};

} // namespace ns3

#endif // COMMUNICATION_TOPOLOGY_MANAGER_H
