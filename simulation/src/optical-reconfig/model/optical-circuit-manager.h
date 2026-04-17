#ifndef OPTICAL_CIRCUIT_MANAGER_H
#define OPTICAL_CIRCUIT_MANAGER_H

#include "optical-types.h"
#include "ns3/ipv4.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <map>
#include <utility>
#include <vector>

namespace ns3 {

class OpticalCircuitManager : public Object
{
public:
  static TypeId GetTypeId (void);

  OpticalCircuitManager ();
  ~OpticalCircuitManager () override;

  void RegisterNode (uint32_t nodeId, uint32_t gpuCount = 0, bool opticalPortAvailable = true);
  void SetNodeAlive (uint32_t nodeId, bool alive);
  void SetNodeOpticalPortAvailable (uint32_t nodeId, bool available);

  void RegisterCandidateCircuit (uint32_t a,
                                 uint32_t b,
                                 Ptr<Ipv4> ipv4A,
                                 uint32_t interfaceIndexA,
                                 Ptr<Ipv4> ipv4B,
                                 uint32_t interfaceIndexB,
                                 bool active,
                                 bool backup,
                                 bool loopback,
                                 Time setupDelay);

  bool HasCircuit (uint32_t a, uint32_t b) const;
  bool IsCircuitActive (uint32_t a, uint32_t b) const;
  CircuitState GetCircuitState (uint32_t a, uint32_t b) const;

  bool ActivateCircuit (uint32_t a, uint32_t b);
  bool DeactivateCircuit (uint32_t a, uint32_t b);
  void DeactivateCircuitsForNode (uint32_t nodeId);

  std::vector<NodeState> GetNodeStates () const;
  std::vector<CircuitState> GetCircuitStates () const;

private:
  struct InterfaceHandle
  {
    Ptr<Ipv4> ipv4;
    uint32_t interfaceIndex = 0;
  };

  struct ManagedCircuit
  {
    CircuitState state;
    InterfaceHandle endpointA;
    InterfaceHandle endpointB;
  };

  void EnsureNode (uint32_t nodeId);
  bool CanActivateCircuit (const ManagedCircuit& circuit) const;
  void SetInterfaceState (const InterfaceHandle& endpoint, bool up) const;

  std::vector<NodeState> m_nodes;
  std::map<std::pair<uint32_t, uint32_t>, ManagedCircuit> m_circuits;
};

} // namespace ns3

#endif // OPTICAL_CIRCUIT_MANAGER_H
