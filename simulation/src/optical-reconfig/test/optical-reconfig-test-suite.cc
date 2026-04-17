/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/communication-topology-manager.h"
#include "ns3/optical-circuit-manager.h"
#include "ns3/optical-reconfig-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/reconfig-orchestrator.h"
#include "ns3/ring-bypass-policy.h"
#include "ns3/test.h"

using namespace ns3;

namespace {

CircuitState
MakeCircuitState (uint32_t a, uint32_t b, bool active, bool backup)
{
  CircuitState circuit;
  circuit.a = a;
  circuit.b = b;
  circuit.active = active;
  circuit.backup = backup;
  circuit.loopback = false;
  circuit.setup_delay_ns = 0;
  return circuit;
}

RingNeighbor
MakeRingNeighbor (uint32_t prev, uint32_t next)
{
  RingNeighbor neighbor;
  neighbor.prev = prev;
  neighbor.next = next;
  return neighbor;
}

} // namespace

class RingBypassPolicyPlanTestCase : public TestCase
{
public:
  RingBypassPolicyPlanTestCase ()
    : TestCase ("RingBypassPolicy should produce the expected bypass plan")
  {
  }

private:
  void DoRun (void) override
  {
    ClusterState clusterState;
    clusterState.nodes.resize (3);
    for (uint32_t i = 0; i < 3; ++i)
      {
        clusterState.nodes[i].node_id = i;
        clusterState.nodes[i].alive = true;
        clusterState.nodes[i].optical_port_available = true;
      }

    clusterState.circuits.push_back (MakeCircuitState (0, 1, true, false));
    clusterState.circuits.push_back (MakeCircuitState (1, 2, true, false));
    clusterState.circuits.push_back (MakeCircuitState (0, 2, false, true));

    clusterState.ring.resize (3);
    clusterState.ring[0] = MakeRingNeighbor (2, 1);
    clusterState.ring[1] = MakeRingNeighbor (0, 2);
    clusterState.ring[2] = MakeRingNeighbor (1, 0);

    FaultEvent event;
    event.type = FaultEvent::NODE;
    event.node_id = 1;

    Ptr<RingBypassPolicy> policy = CreateObject<RingBypassPolicy> ();
    ReconfigPlan plan = policy->ComputePlan (clusterState, event);

    NS_TEST_ASSERT_MSG_EQ (plan.links_to_remove.size (), 2u, "Expected two links to remove");
    NS_TEST_ASSERT_MSG_EQ (plan.links_to_add.size (), 1u, "Expected one bypass link");
    std::pair<uint32_t, uint32_t> bypass = MakeCanonicalLink (0, 2);
    NS_TEST_ASSERT_MSG_EQ (plan.links_to_add[0].first, bypass.first, "Unexpected bypass link source");
    NS_TEST_ASSERT_MSG_EQ (plan.links_to_add[0].second, bypass.second, "Unexpected bypass link destination");
    NS_TEST_ASSERT_MSG_EQ (plan.new_ring[1].prev, OPTICAL_INVALID_NODE_ID, "Failed node should be detached");
    NS_TEST_ASSERT_MSG_EQ (plan.new_ring[0].next, 2u, "Node 0 should now point to node 2");
    NS_TEST_ASSERT_MSG_EQ (plan.new_ring[2].prev, 0u, "Node 2 should now point back to node 0");
  }
};

class RingBypassPolicyUnavailablePortTestCase : public TestCase
{
public:
  RingBypassPolicyUnavailablePortTestCase ()
    : TestCase ("RingBypassPolicy should not create a bypass when a port is unavailable")
  {
  }

private:
  void DoRun (void) override
  {
    ClusterState clusterState;
    clusterState.nodes.resize (3);
    for (uint32_t i = 0; i < 3; ++i)
      {
        clusterState.nodes[i].node_id = i;
        clusterState.nodes[i].alive = true;
        clusterState.nodes[i].optical_port_available = true;
      }
    clusterState.nodes[0].optical_port_available = false;

    clusterState.circuits.push_back (MakeCircuitState (0, 1, true, false));
    clusterState.circuits.push_back (MakeCircuitState (1, 2, true, false));
    clusterState.circuits.push_back (MakeCircuitState (0, 2, false, true));

    clusterState.ring.resize (3);
    clusterState.ring[0] = MakeRingNeighbor (2, 1);
    clusterState.ring[1] = MakeRingNeighbor (0, 2);
    clusterState.ring[2] = MakeRingNeighbor (1, 0);

    FaultEvent event;
    event.type = FaultEvent::NODE;
    event.node_id = 1;

    Ptr<RingBypassPolicy> policy = CreateObject<RingBypassPolicy> ();
    ReconfigPlan plan = policy->ComputePlan (clusterState, event);

    NS_TEST_ASSERT_MSG_EQ (plan.links_to_add.size (), 0u, "No bypass link should be created");
    NS_TEST_ASSERT_MSG_NE (plan.reason.find ("unavailable"), std::string::npos, "Plan should explain the failure");
  }
};

class OrchestratorDelayTestCase : public TestCase
{
public:
  OrchestratorDelayTestCase ()
    : TestCase ("ReconfigOrchestrator should respect reconfiguration delay")
  {
  }

private:
  void CheckBeforeBypass (void);
  void CheckAfterBypass (void);
  void DoRun (void) override;

  Ptr<ReconfigOrchestrator> m_orchestrator;
};

void
OrchestratorDelayTestCase::CheckBeforeBypass (void)
{
  Ptr<OpticalCircuitManager> circuitManager = m_orchestrator->GetCircuitManager ();
  NS_TEST_ASSERT_MSG_EQ (circuitManager->IsCircuitActive (0, 1), false, "Fault isolation should tear down 0-1 immediately");
  NS_TEST_ASSERT_MSG_EQ (circuitManager->IsCircuitActive (1, 2), false, "Fault isolation should tear down 1-2 immediately");
  NS_TEST_ASSERT_MSG_EQ (circuitManager->IsCircuitActive (0, 2), false, "Bypass should still be down before the delay expires");
  NS_TEST_ASSERT_MSG_EQ (m_orchestrator->IsReconfiguring (), true, "Orchestrator should still be reconfiguring");
}

void
OrchestratorDelayTestCase::CheckAfterBypass (void)
{
  Ptr<OpticalCircuitManager> circuitManager = m_orchestrator->GetCircuitManager ();
  Ptr<CommunicationTopologyManager> topologyManager = m_orchestrator->GetTopologyManager ();

  NS_TEST_ASSERT_MSG_EQ (circuitManager->IsCircuitActive (0, 2), true, "Bypass should be active after the delay");
  NS_TEST_ASSERT_MSG_EQ (topologyManager->GetRingNeighbor (1).prev,
                         OPTICAL_INVALID_NODE_ID,
                         "Failed node should be detached from the ring");
  NS_TEST_ASSERT_MSG_EQ (topologyManager->GetRingNeighbor (0).next, 2u, "Ring should bypass node 1");
  NS_TEST_ASSERT_MSG_EQ (m_orchestrator->IsReconfiguring (), false, "Orchestrator should have finished");
}

void
OrchestratorDelayTestCase::DoRun (void)
{
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

  NodeContainer nodes;
  nodes.Create (3);

  InternetStackHelper internet;
  internet.Install (nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer d01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d02 = p2p.Install (nodes.Get (0), nodes.Get (2));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (d01);
  ipv4.SetBase ("10.2.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);
  ipv4.SetBase ("10.3.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = ipv4.Assign (d02);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  OpticalReconfigHelper helper;
  helper.RegisterNode (0);
  helper.RegisterNode (1);
  helper.RegisterNode (2);
  helper.SetRingOrder ({0, 1, 2});
  helper.SetReconfigDelay (MilliSeconds (5));
  helper.RegisterCandidateCircuit (0, 1, i01.Get (0).first, i01.Get (0).second, i01.Get (1).first, i01.Get (1).second, true);
  helper.RegisterCandidateCircuit (1, 2, i12.Get (0).first, i12.Get (0).second, i12.Get (1).first, i12.Get (1).second, true);
  helper.RegisterCandidateCircuit (0, 2, i02.Get (0).first, i02.Get (0).second, i02.Get (1).first, i02.Get (1).second, false, true);
  m_orchestrator = helper.Install ();

  Simulator::Schedule (MilliSeconds (1),
                       &ReconfigOrchestrator::InjectNodeFault,
                       m_orchestrator,
                       1);
  Simulator::Schedule (MilliSeconds (3),
                       &OrchestratorDelayTestCase::CheckBeforeBypass,
                       this);
  Simulator::Schedule (MilliSeconds (7),
                       &OrchestratorDelayTestCase::CheckAfterBypass,
                       this);

  Simulator::Stop (MilliSeconds (10));
  Simulator::Run ();
  Simulator::Destroy ();
}

class OpticalReconfigTestSuite : public TestSuite
{
public:
  OpticalReconfigTestSuite ()
    : TestSuite ("optical-reconfig", UNIT)
  {
    AddTestCase (new RingBypassPolicyPlanTestCase, TestCase::QUICK);
    AddTestCase (new RingBypassPolicyUnavailablePortTestCase, TestCase::QUICK);
    AddTestCase (new OrchestratorDelayTestCase, TestCase::QUICK);
  }
};

static OpticalReconfigTestSuite g_opticalReconfigTestSuite;
