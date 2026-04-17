#ifndef RECONFIG_POLICY_H
#define RECONFIG_POLICY_H

#include "optical-types.h"
#include "ns3/object.h"

namespace ns3 {

class ReconfigPolicy : public Object
{
public:
  static TypeId GetTypeId (void);

  ~ReconfigPolicy () override;

  virtual ReconfigPlan ComputePlan (const ClusterState& clusterState, const FaultEvent& faultEvent) const = 0;
};

} // namespace ns3

#endif // RECONFIG_POLICY_H
