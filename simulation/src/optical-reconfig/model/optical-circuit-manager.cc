#include "optical-circuit-manager.h"

#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OpticalCircuitManager");
NS_OBJECT_ENSURE_REGISTERED (OpticalCircuitManager);

TypeId
OpticalCircuitManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OpticalCircuitManager")
    .SetParent<Object> ()
    .SetGroupName ("OpticalReconfig")
    .AddConstructor<OpticalCircuitManager> ();
  return tid;
}

OpticalCircuitManager::OpticalCircuitManager () = default;

OpticalCircuitManager::~OpticalCircuitManager () = default;

void
OpticalCircuitManager::RegisterNode (uint32_t nodeId, uint32_t gpuCount, bool opticalPortAvailable)
{
  EnsureNode (nodeId);
  NodeState& node = m_nodes[nodeId];
  node.node_id = nodeId;
  node.alive = true;
  node.optical_port_available = opticalPortAvailable;
  node.gpu_alive.assign (gpuCount, true);
}

void
OpticalCircuitManager::SetNodeAlive (uint32_t nodeId, bool alive)
{
  EnsureNode (nodeId);
  m_nodes[nodeId].alive = alive;
}

void
OpticalCircuitManager::SetNodeOpticalPortAvailable (uint32_t nodeId, bool available)
{
  EnsureNode (nodeId);
  m_nodes[nodeId].optical_port_available = available;
}

void
OpticalCircuitManager::RegisterCandidateCircuit (uint32_t a,
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
  EnsureNode (a);
  EnsureNode (b);

  std::pair<uint32_t, uint32_t> key = MakeCanonicalLink (a, b);
  ManagedCircuit& managed = m_circuits[key];
  managed.state.a = key.first;
  managed.state.b = key.second;
  managed.state.active = active;
  managed.state.backup = backup;
  managed.state.loopback = loopback;
  managed.state.setup_delay_ns = static_cast<uint64_t> (setupDelay.GetNanoSeconds ());
  if (key.first == a)
    {
      managed.endpointA.ipv4 = ipv4A;
      managed.endpointA.interfaceIndex = interfaceIndexA;
      managed.endpointB.ipv4 = ipv4B;
      managed.endpointB.interfaceIndex = interfaceIndexB;
    }
  else
    {
      managed.endpointA.ipv4 = ipv4B;
      managed.endpointA.interfaceIndex = interfaceIndexB;
      managed.endpointB.ipv4 = ipv4A;
      managed.endpointB.interfaceIndex = interfaceIndexA;
    }

  SetInterfaceState (managed.endpointA, active);
  SetInterfaceState (managed.endpointB, active);
}

bool
OpticalCircuitManager::HasCircuit (uint32_t a, uint32_t b) const
{
  return m_circuits.find (MakeCanonicalLink (a, b)) != m_circuits.end ();
}

bool
OpticalCircuitManager::IsCircuitActive (uint32_t a, uint32_t b) const
{
  auto it = m_circuits.find (MakeCanonicalLink (a, b));
  if (it == m_circuits.end ())
    {
      return false;
    }
  return it->second.state.active;
}

CircuitState
OpticalCircuitManager::GetCircuitState (uint32_t a, uint32_t b) const
{
  auto it = m_circuits.find (MakeCanonicalLink (a, b));
  if (it == m_circuits.end ())
    {
      return CircuitState ();
    }
  return it->second.state;
}

bool
OpticalCircuitManager::ActivateCircuit (uint32_t a, uint32_t b)
{
  auto it = m_circuits.find (MakeCanonicalLink (a, b));
  if (it == m_circuits.end ())
    {
      return false;
    }

  ManagedCircuit& circuit = it->second;
  if (!CanActivateCircuit (circuit))
    {
      return false;
    }

  circuit.state.active = true;
  SetInterfaceState (circuit.endpointA, true);
  SetInterfaceState (circuit.endpointB, true);
  return true;
}

bool
OpticalCircuitManager::DeactivateCircuit (uint32_t a, uint32_t b)
{
  auto it = m_circuits.find (MakeCanonicalLink (a, b));
  if (it == m_circuits.end ())
    {
      return false;
    }

  ManagedCircuit& circuit = it->second;
  circuit.state.active = false;
  SetInterfaceState (circuit.endpointA, false);
  SetInterfaceState (circuit.endpointB, false);
  return true;
}

void
OpticalCircuitManager::DeactivateCircuitsForNode (uint32_t nodeId)
{
  for (auto& entry : m_circuits)
    {
      if (entry.second.state.a == nodeId || entry.second.state.b == nodeId)
        {
          entry.second.state.active = false;
          SetInterfaceState (entry.second.endpointA, false);
          SetInterfaceState (entry.second.endpointB, false);
        }
    }
}

std::vector<NodeState>
OpticalCircuitManager::GetNodeStates () const
{
  return m_nodes;
}

std::vector<CircuitState>
OpticalCircuitManager::GetCircuitStates () const
{
  std::vector<CircuitState> circuits;
  circuits.reserve (m_circuits.size ());
  for (const auto& entry : m_circuits)
    {
      circuits.push_back (entry.second.state);
    }
  return circuits;
}

void
OpticalCircuitManager::EnsureNode (uint32_t nodeId)
{
  if (nodeId >= m_nodes.size ())
    {
      uint32_t oldSize = static_cast<uint32_t> (m_nodes.size ());
      m_nodes.resize (nodeId + 1);
      for (uint32_t i = oldSize; i < m_nodes.size (); ++i)
        {
          m_nodes[i].node_id = i;
        }
    }
}

bool
OpticalCircuitManager::CanActivateCircuit (const ManagedCircuit& circuit) const
{
  if (circuit.state.a >= m_nodes.size () || circuit.state.b >= m_nodes.size ())
    {
      return false;
    }

  const NodeState& nodeA = m_nodes[circuit.state.a];
  const NodeState& nodeB = m_nodes[circuit.state.b];
  return nodeA.alive && nodeB.alive && nodeA.optical_port_available && nodeB.optical_port_available;
}

void
OpticalCircuitManager::SetInterfaceState (const InterfaceHandle& endpoint, bool up) const
{
  if (endpoint.ipv4 == nullptr)
    {
      return;
    }

  if (up)
    {
      endpoint.ipv4->SetUp (endpoint.interfaceIndex);
    }
  else
    {
      endpoint.ipv4->SetDown (endpoint.interfaceIndex);
    }
}

} // namespace ns3
