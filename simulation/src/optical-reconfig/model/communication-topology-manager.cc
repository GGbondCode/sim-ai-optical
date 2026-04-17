#include "communication-topology-manager.h"

#include "ns3/log.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("CommunicationTopologyManager");
NS_OBJECT_ENSURE_REGISTERED (CommunicationTopologyManager);

TypeId
CommunicationTopologyManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CommunicationTopologyManager")
    .SetParent<Object> ()
    .SetGroupName ("OpticalReconfig")
    .AddConstructor<CommunicationTopologyManager> ();
  return tid;
}

CommunicationTopologyManager::CommunicationTopologyManager () = default;

CommunicationTopologyManager::~CommunicationTopologyManager () = default;

void
CommunicationTopologyManager::SetRingOrder (const std::vector<uint32_t>& ringOrder)
{
  m_ringOrder = ringOrder;
  m_ring = BuildRingNeighbors (ringOrder);
}

void
CommunicationTopologyManager::ApplyRing (const std::vector<RingNeighbor>& ring)
{
  m_ring = ring;
  m_ringOrder = BuildRingOrder (ring);
}

std::vector<uint32_t>
CommunicationTopologyManager::GetRingOrder () const
{
  return m_ringOrder;
}

const std::vector<RingNeighbor>&
CommunicationTopologyManager::GetRingNeighbors () const
{
  return m_ring;
}

RingNeighbor
CommunicationTopologyManager::GetRingNeighbor (uint32_t nodeId) const
{
  if (nodeId >= m_ring.size ())
    {
      return RingNeighbor ();
    }
  return m_ring[nodeId];
}

std::string
CommunicationTopologyManager::DescribeRing () const
{
  return DescribeRing (m_ring);
}

std::vector<uint32_t>
CommunicationTopologyManager::BuildRingOrder (const std::vector<RingNeighbor>& ring)
{
  std::vector<uint32_t> order;
  if (ring.empty ())
    {
      return order;
    }

  uint32_t start = OPTICAL_INVALID_NODE_ID;
  for (uint32_t nodeId = 0; nodeId < ring.size (); ++nodeId)
    {
      if (IsValidNodeId (ring[nodeId].next) && IsValidNodeId (ring[nodeId].prev))
        {
          start = nodeId;
          break;
        }
    }

  if (!IsValidNodeId (start))
    {
      return order;
    }

  std::set<uint32_t> visited;
  uint32_t current = start;
  while (IsValidNodeId (current) && visited.insert (current).second)
    {
      order.push_back (current);
      uint32_t next = ring[current].next;
      if (!IsValidNodeId (next) || next >= ring.size ())
        {
          break;
        }
      current = next;
    }

  return order;
}

std::string
CommunicationTopologyManager::DescribeRing (const std::vector<RingNeighbor>& ring)
{
  std::vector<uint32_t> order = BuildRingOrder (ring);
  if (order.empty ())
    {
      return "<empty>";
    }

  std::ostringstream oss;
  for (uint32_t i = 0; i < order.size (); ++i)
    {
      if (i != 0)
        {
          oss << " -> ";
        }
      oss << "N" << order[i];
    }
  oss << " -> N" << order.front ();
  return oss.str ();
}

std::vector<RingNeighbor>
CommunicationTopologyManager::BuildRingNeighbors (const std::vector<uint32_t>& ringOrder)
{
  if (ringOrder.empty ())
    {
      return std::vector<RingNeighbor> ();
    }

  uint32_t maxNodeId = 0;
  for (uint32_t nodeId : ringOrder)
    {
      maxNodeId = std::max (maxNodeId, nodeId);
    }

  std::vector<RingNeighbor> ring (maxNodeId + 1);
  for (uint32_t i = 0; i < ring.size (); ++i)
    {
      ring[i].prev = OPTICAL_INVALID_NODE_ID;
      ring[i].next = OPTICAL_INVALID_NODE_ID;
    }

  if (ringOrder.size () == 1)
    {
      uint32_t only = ringOrder.front ();
      ring[only].prev = only;
      ring[only].next = only;
      return ring;
    }

  for (uint32_t i = 0; i < ringOrder.size (); ++i)
    {
      uint32_t current = ringOrder[i];
      uint32_t prev = ringOrder[(i + ringOrder.size () - 1) % ringOrder.size ()];
      uint32_t next = ringOrder[(i + 1) % ringOrder.size ()];
      ring[current].prev = prev;
      ring[current].next = next;
    }

  return ring;
}

} // namespace ns3
