#ifndef RECONFIG_ORCHESTRATOR_H
#define RECONFIG_ORCHESTRATOR_H

#include "communication-topology-manager.h"
#include "fault-manager.h"
#include "optical-circuit-manager.h"
#include "reconfig-policy.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <string>

namespace ns3 {

class ReconfigOrchestrator : public Object
{
public:
  static TypeId GetTypeId (void);

  ReconfigOrchestrator ();
  ~ReconfigOrchestrator () override;

  void SetFaultManager (Ptr<FaultManager> faultManager);
  void SetCircuitManager (Ptr<OpticalCircuitManager> circuitManager);
  void SetTopologyManager (Ptr<CommunicationTopologyManager> topologyManager);
  void SetPolicy (Ptr<ReconfigPolicy> policy);
  void SetReconfigDelay (Time delay);

  Ptr<FaultManager> GetFaultManager () const;
  Ptr<OpticalCircuitManager> GetCircuitManager () const;
  Ptr<CommunicationTopologyManager> GetTopologyManager () const;
  Ptr<ReconfigPolicy> GetPolicy () const;
  Time GetReconfigDelay () const;

  ClusterState GetClusterState () const;
  const ReconfigPlan& GetLastReconfigPlan () const;
  bool IsReconfiguring () const;
  std::string DescribeCurrentRing () const;

  void InjectNodeFault (uint32_t nodeId);

private:
  void HandleFault (FaultEvent event);
  void FinishReconfiguration (ReconfigPlan plan);
  void RecomputeRoutes (const std::string& reason) const;
  std::string DescribePlan (const ReconfigPlan& plan) const;

  Ptr<FaultManager> m_faultManager;
  Ptr<OpticalCircuitManager> m_circuitManager;
  Ptr<CommunicationTopologyManager> m_topologyManager;
  Ptr<ReconfigPolicy> m_policy;
  Time m_reconfigDelay;
  bool m_reconfiguring;
  ReconfigPlan m_lastPlan;
};

} // namespace ns3

#endif // RECONFIG_ORCHESTRATOR_H
