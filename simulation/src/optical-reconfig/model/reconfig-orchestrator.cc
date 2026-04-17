#include "reconfig-orchestrator.h"

#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ReconfigOrchestrator");
NS_OBJECT_ENSURE_REGISTERED (ReconfigOrchestrator);

namespace {

std::string
DescribeLinks (const std::vector<std::pair<uint32_t, uint32_t>>& links)
{
  if (links.empty ())
    {
      return "<none>";
    }

  std::ostringstream oss;
  for (uint32_t i = 0; i < links.size (); ++i)
    {
      if (i != 0)
        {
          oss << ", ";
        }
      oss << "N" << links[i].first << "-N" << links[i].second;
    }
  return oss.str ();
}

} // namespace

TypeId
ReconfigOrchestrator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ReconfigOrchestrator")
    .SetParent<Object> ()
    .SetGroupName ("OpticalReconfig")
    .AddConstructor<ReconfigOrchestrator> ();
  return tid;
}

ReconfigOrchestrator::ReconfigOrchestrator ()
  : m_reconfigDelay (MilliSeconds (1)),
    m_reconfiguring (false)
{
}

ReconfigOrchestrator::~ReconfigOrchestrator () = default;

void
ReconfigOrchestrator::SetFaultManager (Ptr<FaultManager> faultManager)
{
  m_faultManager = faultManager;
  if (m_faultManager != nullptr)
    {
      m_faultManager->SetFaultCallback (MakeCallback (&ReconfigOrchestrator::HandleFault, this));
    }
}

void
ReconfigOrchestrator::SetCircuitManager (Ptr<OpticalCircuitManager> circuitManager)
{
  m_circuitManager = circuitManager;
}

void
ReconfigOrchestrator::SetTopologyManager (Ptr<CommunicationTopologyManager> topologyManager)
{
  m_topologyManager = topologyManager;
}

void
ReconfigOrchestrator::SetPolicy (Ptr<ReconfigPolicy> policy)
{
  m_policy = policy;
}

void
ReconfigOrchestrator::SetReconfigDelay (Time delay)
{
  m_reconfigDelay = delay;
}

Ptr<FaultManager>
ReconfigOrchestrator::GetFaultManager () const
{
  return m_faultManager;
}

Ptr<OpticalCircuitManager>
ReconfigOrchestrator::GetCircuitManager () const
{
  return m_circuitManager;
}

Ptr<CommunicationTopologyManager>
ReconfigOrchestrator::GetTopologyManager () const
{
  return m_topologyManager;
}

Ptr<ReconfigPolicy>
ReconfigOrchestrator::GetPolicy () const
{
  return m_policy;
}

Time
ReconfigOrchestrator::GetReconfigDelay () const
{
  return m_reconfigDelay;
}

ClusterState
ReconfigOrchestrator::GetClusterState () const
{
  ClusterState clusterState;
  if (m_circuitManager != nullptr)
    {
      clusterState.nodes = m_circuitManager->GetNodeStates ();
      clusterState.circuits = m_circuitManager->GetCircuitStates ();
    }
  if (m_topologyManager != nullptr)
    {
      clusterState.ring = m_topologyManager->GetRingNeighbors ();
    }
  return clusterState;
}

const ReconfigPlan&
ReconfigOrchestrator::GetLastReconfigPlan () const
{
  return m_lastPlan;
}

bool
ReconfigOrchestrator::IsReconfiguring () const
{
  return m_reconfiguring;
}

std::string
ReconfigOrchestrator::DescribeCurrentRing () const
{
  if (m_topologyManager == nullptr)
    {
      return "<uninitialized>";
    }
  return m_topologyManager->DescribeRing ();
}

void
ReconfigOrchestrator::InjectNodeFault (uint32_t nodeId)
{
  if (m_faultManager != nullptr)
    {
      m_faultManager->InjectNodeFault (nodeId);
    }
}

void
ReconfigOrchestrator::HandleFault (FaultEvent event)
{
  if (m_circuitManager == nullptr || m_topologyManager == nullptr || m_policy == nullptr)
    {
      NS_LOG_UNCOND ("[OpticalReconfig] fault ignored because orchestrator is not fully initialized");
      return;
    }

  if (m_reconfiguring)
    {
      NS_LOG_UNCOND ("[OpticalReconfig] fault ignored because a reconfiguration is already running");
      return;
    }

  NS_LOG_UNCOND ("[OpticalReconfig] Fault injected at " << Simulator::Now ().GetMilliSeconds ()
                                                        << " ms: node N" << event.node_id);
  NS_LOG_UNCOND ("[OpticalReconfig] Ring before fault: " << DescribeCurrentRing ());

  m_circuitManager->SetNodeAlive (event.node_id, false);
  m_circuitManager->SetNodeOpticalPortAvailable (event.node_id, false);
  m_circuitManager->DeactivateCircuitsForNode (event.node_id);

  ClusterState clusterState = GetClusterState ();
  m_lastPlan = m_policy->ComputePlan (clusterState, event);

  NS_LOG_UNCOND ("[OpticalReconfig] Reconfiguration start at " << Simulator::Now ().GetMilliSeconds ()
                                                               << " ms");
  NS_LOG_UNCOND ("[OpticalReconfig] " << DescribePlan (m_lastPlan));

  for (const auto& link : m_lastPlan.links_to_remove)
    {
      if (m_circuitManager->DeactivateCircuit (link.first, link.second))
        {
          NS_LOG_UNCOND ("[OpticalReconfig] Link removed at " << Simulator::Now ().GetMilliSeconds ()
                                                              << " ms: N" << link.first << "-N" << link.second);
        }
    }

  RecomputeRoutes ("fault isolation");

  m_reconfiguring = true;
  Simulator::Schedule (m_reconfigDelay,
                       &ReconfigOrchestrator::FinishReconfiguration,
                       this,
                       m_lastPlan);
}

void
ReconfigOrchestrator::FinishReconfiguration (ReconfigPlan plan)
{
  if (m_circuitManager != nullptr)
    {
      for (const auto& link : plan.links_to_add)
        {
          if (m_circuitManager->ActivateCircuit (link.first, link.second))
            {
              NS_LOG_UNCOND ("[OpticalReconfig] Link added at " << Simulator::Now ().GetMilliSeconds ()
                                                                << " ms: N" << link.first << "-N" << link.second);
            }
          else
            {
              NS_LOG_UNCOND ("[OpticalReconfig] Failed to activate link N" << link.first
                                                                           << "-N" << link.second);
            }
        }
    }

  if (m_topologyManager != nullptr)
    {
      m_topologyManager->ApplyRing (plan.new_ring);
    }

  RecomputeRoutes ("reconfiguration");

  NS_LOG_UNCOND ("[OpticalReconfig] Ring after reconfiguration: " << DescribeCurrentRing ());
  NS_LOG_UNCOND ("[OpticalReconfig] Reconfiguration complete at "
                 << Simulator::Now ().GetMilliSeconds () << " ms");

  m_reconfiguring = false;
}

void
ReconfigOrchestrator::RecomputeRoutes (const std::string& reason) const
{
  Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
  NS_LOG_UNCOND ("[OpticalReconfig] Routing recomputed at " << Simulator::Now ().GetMilliSeconds ()
                                                            << " ms (" << reason << ")");
}

std::string
ReconfigOrchestrator::DescribePlan (const ReconfigPlan& plan) const
{
  std::ostringstream oss;
  oss << "Plan: remove=[" << DescribeLinks (plan.links_to_remove)
      << "], add=[" << DescribeLinks (plan.links_to_add)
      << "], ring=" << CommunicationTopologyManager::DescribeRing (plan.new_ring)
      << ", reason=\"" << plan.reason << "\"";
  return oss.str ();
}

} // namespace ns3
