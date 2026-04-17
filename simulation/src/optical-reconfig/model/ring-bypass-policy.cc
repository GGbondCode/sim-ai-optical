#include "ring-bypass-policy.h"

#include "ns3/log.h"

#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RingBypassPolicy");
NS_OBJECT_ENSURE_REGISTERED (RingBypassPolicy);

namespace {

std::string
MakeBypassReason (uint32_t prev, uint32_t failed, uint32_t next)
{
  std::ostringstream oss;
  oss << "node " << failed << " bypassed via " << prev << "-" << next;
  return oss.str ();
}

std::string
MakeUnavailableReason (uint32_t failed)
{
  std::ostringstream oss;
  oss << "bypass unavailable for node " << failed;
  return oss.str ();
}

} // namespace

TypeId
ReconfigPolicy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ReconfigPolicy")
    .SetParent<Object> ()
    .SetGroupName ("OpticalReconfig");
  return tid;
}

ReconfigPolicy::~ReconfigPolicy () = default;

TypeId
RingBypassPolicy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RingBypassPolicy")
    .SetParent<ReconfigPolicy> ()
    .SetGroupName ("OpticalReconfig")
    .AddConstructor<RingBypassPolicy> ();
  return tid;
}

RingBypassPolicy::RingBypassPolicy () = default;

RingBypassPolicy::~RingBypassPolicy () = default;

ReconfigPlan
RingBypassPolicy::ComputePlan (const ClusterState& clusterState, const FaultEvent& faultEvent) const
{
  ReconfigPlan plan;
  plan.new_ring = clusterState.ring;

  if (faultEvent.type != FaultEvent::NODE || faultEvent.node_id >= clusterState.ring.size ())
    {
      plan.reason = "unsupported fault event";
      return plan;
    }

  uint32_t failed = faultEvent.node_id;
  RingNeighbor failedNeighbor = clusterState.ring[failed];
  if (!IsValidNodeId (failedNeighbor.prev) || !IsValidNodeId (failedNeighbor.next))
    {
      plan.reason = MakeUnavailableReason (failed);
      return plan;
    }

  uint32_t prev = failedNeighbor.prev;
  uint32_t next = failedNeighbor.next;
  plan.links_to_remove.push_back (MakeCanonicalLink (prev, failed));
  plan.links_to_remove.push_back (MakeCanonicalLink (failed, next));

  if (failed < plan.new_ring.size ())
    {
      plan.new_ring[failed].prev = OPTICAL_INVALID_NODE_ID;
      plan.new_ring[failed].next = OPTICAL_INVALID_NODE_ID;
    }

  if (!IsNodeHealthy (clusterState, prev) ||
      !IsNodeHealthy (clusterState, next) ||
      !HasCandidateCircuit (clusterState, prev, next))
    {
      if (prev < plan.new_ring.size ())
        {
          plan.new_ring[prev].next = OPTICAL_INVALID_NODE_ID;
        }
      if (next < plan.new_ring.size ())
        {
          plan.new_ring[next].prev = OPTICAL_INVALID_NODE_ID;
        }
      plan.reason = MakeUnavailableReason (failed);
      return plan;
    }

  plan.links_to_add.push_back (MakeCanonicalLink (prev, next));
  plan.new_ring[prev].next = next;
  plan.new_ring[next].prev = prev;
  plan.reason = MakeBypassReason (prev, failed, next);
  return plan;
}

bool
RingBypassPolicy::IsNodeHealthy (const ClusterState& clusterState, uint32_t nodeId) const
{
  return nodeId < clusterState.nodes.size () &&
         clusterState.nodes[nodeId].alive &&
         clusterState.nodes[nodeId].optical_port_available;
}

bool
RingBypassPolicy::HasCandidateCircuit (const ClusterState& clusterState, uint32_t a, uint32_t b) const
{
  std::pair<uint32_t, uint32_t> target = MakeCanonicalLink (a, b);
  for (const CircuitState& circuit : clusterState.circuits)
    {
      if (MakeCanonicalLink (circuit.a, circuit.b) == target)
        {
          return true;
        }
    }
  return false;
}

} // namespace ns3
