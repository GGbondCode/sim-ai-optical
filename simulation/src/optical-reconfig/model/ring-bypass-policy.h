#ifndef RING_BYPASS_POLICY_H
#define RING_BYPASS_POLICY_H

#include "reconfig-policy.h"

namespace ns3 {

class RingBypassPolicy : public ReconfigPolicy
{
public:
  static TypeId GetTypeId (void);

  RingBypassPolicy ();
  ~RingBypassPolicy () override;

  ReconfigPlan ComputePlan (const ClusterState& clusterState, const FaultEvent& faultEvent) const override;

private:
  bool IsNodeHealthy (const ClusterState& clusterState, uint32_t nodeId) const;
  bool HasCandidateCircuit (const ClusterState& clusterState, uint32_t a, uint32_t b) const;
};

} // namespace ns3

#endif // RING_BYPASS_POLICY_H
