#include "optical-reconfig-helper.h"

#include "ns3/communication-topology-manager.h"
#include "ns3/fault-manager.h"
#include "ns3/log.h"
#include "ns3/optical-circuit-manager.h"
#include "ns3/ring-bypass-policy.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OpticalReconfigHelper");

OpticalReconfigHelper::OpticalReconfigHelper()
  : m_reconfigDelay (MilliSeconds (1))
{
  m_factory.SetTypeId("ns3::ReconfigOrchestrator");
}

void
OpticalReconfigHelper::RegisterNode (uint32_t nodeId, uint32_t gpuCount, bool opticalPortAvailable)
{
  NodeRegistration registration;
  registration.gpuCount = gpuCount;
  registration.opticalPortAvailable = opticalPortAvailable;
  m_nodes[nodeId] = registration;
}

void
OpticalReconfigHelper::SetRingOrder (const std::vector<uint32_t>& ringOrder)
{
  m_ringOrder = ringOrder;
}

void
OpticalReconfigHelper::SetReconfigDelay (Time delay)
{
  m_reconfigDelay = delay;
}

void
OpticalReconfigHelper::SetPolicy (Ptr<ReconfigPolicy> policy)
{
  m_policy = policy;
}

void
OpticalReconfigHelper::RegisterCandidateCircuit (uint32_t a,
                                                 uint32_t b,
                                                 Ptr<Ipv4> ipv4A,
                                                 uint32_t interfaceIndexA,
                                                 Ptr<Ipv4> ipv4B,
                                                 uint32_t interfaceIndexB,
                                                 bool active,
                                                 bool backup,
                                                 bool loopback,
                                                 Time setupDelay)
{
  CircuitRegistration registration;
  registration.a = a;
  registration.b = b;
  registration.ipv4A = ipv4A;
  registration.interfaceIndexA = interfaceIndexA;
  registration.ipv4B = ipv4B;
  registration.interfaceIndexB = interfaceIndexB;
  registration.active = active;
  registration.backup = backup;
  registration.loopback = loopback;
  registration.setupDelay = setupDelay;
  m_circuits.push_back (registration);
}

Ptr<ReconfigOrchestrator>
OpticalReconfigHelper::Install () const
{
  Ptr<ReconfigOrchestrator> orchestrator = m_factory.Create<ReconfigOrchestrator> ();
  Ptr<FaultManager> faultManager = CreateObject<FaultManager> ();
  Ptr<OpticalCircuitManager> circuitManager = CreateObject<OpticalCircuitManager> ();
  Ptr<CommunicationTopologyManager> topologyManager = CreateObject<CommunicationTopologyManager> ();
  Ptr<ReconfigPolicy> policy = m_policy;
  if (policy == nullptr)
    {
      policy = CreateObject<RingBypassPolicy> ();
    }

  for (const auto& entry : m_nodes)
    {
      circuitManager->RegisterNode (entry.first, entry.second.gpuCount, entry.second.opticalPortAvailable);
    }
  for (uint32_t nodeId : m_ringOrder)
    {
      if (m_nodes.find (nodeId) == m_nodes.end ())
        {
          circuitManager->RegisterNode (nodeId);
        }
    }

  for (const CircuitRegistration& circuit : m_circuits)
    {
      circuitManager->RegisterCandidateCircuit (circuit.a,
                                                circuit.b,
                                                circuit.ipv4A,
                                                circuit.interfaceIndexA,
                                                circuit.ipv4B,
                                                circuit.interfaceIndexB,
                                                circuit.active,
                                                circuit.backup,
                                                circuit.loopback,
                                                circuit.setupDelay);
    }

  topologyManager->SetRingOrder (m_ringOrder);

  orchestrator->SetCircuitManager (circuitManager);
  orchestrator->SetTopologyManager (topologyManager);
  orchestrator->SetFaultManager (faultManager);
  orchestrator->SetPolicy (policy);
  orchestrator->SetReconfigDelay (m_reconfigDelay);
  return orchestrator;
}

Ptr<ReconfigOrchestrator>
OpticalReconfigHelper::Create () const
{
  return Install ();
}

} // namespace ns3
