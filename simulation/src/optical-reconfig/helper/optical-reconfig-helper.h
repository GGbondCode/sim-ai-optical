#ifndef OPTICAL_RECONFIG_HELPER_H
#define OPTICAL_RECONFIG_HELPER_H

#include "ns3/ipv4.h"
#include "ns3/object-factory.h"
#include "ns3/ptr.h"
#include "ns3/reconfig-orchestrator.h"
#include "ns3/reconfig-policy.h"

#include <map>
#include <vector>

namespace ns3 {

class OpticalReconfigHelper
{
public:
  OpticalReconfigHelper();
  void RegisterNode (uint32_t nodeId, uint32_t gpuCount = 0, bool opticalPortAvailable = true);
  void SetRingOrder (const std::vector<uint32_t>& ringOrder);
  void SetReconfigDelay (Time delay);
  void SetPolicy (Ptr<ReconfigPolicy> policy);

  void RegisterCandidateCircuit (uint32_t a,
                                 uint32_t b,
                                 Ptr<Ipv4> ipv4A,
                                 uint32_t interfaceIndexA,
                                 Ptr<Ipv4> ipv4B,
                                 uint32_t interfaceIndexB,
                                 bool active,
                                 bool backup = false,
                                 bool loopback = false,
                                 Time setupDelay = NanoSeconds (0));

  Ptr<ReconfigOrchestrator> Install () const;
  Ptr<ReconfigOrchestrator> Create () const;

private:
  struct NodeRegistration
  {
    uint32_t gpuCount = 0;
    bool opticalPortAvailable = true;
  };

  struct CircuitRegistration
  {
    uint32_t a = 0;
    uint32_t b = 0;
    Ptr<Ipv4> ipv4A;
    uint32_t interfaceIndexA = 0;
    Ptr<Ipv4> ipv4B;
    uint32_t interfaceIndexB = 0;
    bool active = false;
    bool backup = false;
    bool loopback = false;
    Time setupDelay = NanoSeconds (0);
  };

  ObjectFactory m_factory;
  std::map<uint32_t, NodeRegistration> m_nodes;
  std::vector<uint32_t> m_ringOrder;
  std::vector<CircuitRegistration> m_circuits;
  Ptr<ReconfigPolicy> m_policy;
  Time m_reconfigDelay;
};

} // namespace ns3

#endif // OPTICAL_RECONFIG_HELPER_H
