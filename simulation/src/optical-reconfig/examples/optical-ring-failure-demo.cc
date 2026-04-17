#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/optical-reconfig-helper.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OpticalRingFailureDemo");

namespace {

void
LogSinkRx (Ptr<const Packet> packet, const Address& address)
{
  InetSocketAddress socketAddress = InetSocketAddress::ConvertFrom (address);
  NS_LOG_UNCOND ("[Demo] Sink received " << packet->GetSize ()
                 << " bytes at " << Simulator::Now ().GetMilliSeconds ()
                 << " ms from " << socketAddress.GetIpv4 ());
}

} // namespace

int
main (int argc, char* argv[])
{
  Time::SetResolution (Time::NS);
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

  Time reconfigDelay = MilliSeconds (5);
  CommandLine cmd (__FILE__);
  cmd.AddValue ("reconfigDelay", "Optical reconfiguration delay", reconfigDelay);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  InternetStackHelper internet;
  internet.Install (nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer d01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d23 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer d30 = p2p.Install (nodes.Get (3), nodes.Get (0));
  NetDeviceContainer d02 = p2p.Install (nodes.Get (0), nodes.Get (2));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (d01);
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (d23);
  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i30 = ipv4.Assign (d30);
  ipv4.SetBase ("10.0.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = ipv4.Assign (d02);

  i01.SetMetric (0, 1);
  i01.SetMetric (1, 1);
  i12.SetMetric (0, 1);
  i12.SetMetric (1, 1);
  i23.SetMetric (0, 1);
  i23.SetMetric (1, 1);
  i30.SetMetric (0, 50);
  i30.SetMetric (1, 50);
  i02.SetMetric (0, 1);
  i02.SetMetric (1, 1);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  OpticalReconfigHelper helper;
  helper.RegisterNode (0);
  helper.RegisterNode (1);
  helper.RegisterNode (2);
  helper.RegisterNode (3);
  helper.SetRingOrder ({0, 1, 2, 3});
  helper.SetReconfigDelay (reconfigDelay);
  helper.RegisterCandidateCircuit (0, 1, i01.Get (0).first, i01.Get (0).second, i01.Get (1).first, i01.Get (1).second, true);
  helper.RegisterCandidateCircuit (1, 2, i12.Get (0).first, i12.Get (0).second, i12.Get (1).first, i12.Get (1).second, true);
  helper.RegisterCandidateCircuit (2, 3, i23.Get (0).first, i23.Get (0).second, i23.Get (1).first, i23.Get (1).second, true);
  helper.RegisterCandidateCircuit (0, 3, i30.Get (1).first, i30.Get (1).second, i30.Get (0).first, i30.Get (0).second, true);
  helper.RegisterCandidateCircuit (0, 2, i02.Get (0).first, i02.Get (0).second, i02.Get (1).first, i02.Get (1).second, false, true);

  Ptr<ReconfigOrchestrator> orchestrator = helper.Install ();

  uint16_t port = 9000;
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                               InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (3));
  sinkApps.Start (MilliSeconds (0));
  sinkApps.Stop (MilliSeconds (35));

  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&LogSinkRx));

  OnOffHelper sender ("ns3::UdpSocketFactory",
                      InetSocketAddress (i23.GetAddress (1), port));
  sender.SetConstantRate (DataRate ("20Mbps"), 512);
  ApplicationContainer senderApps = sender.Install (nodes.Get (0));
  senderApps.Start (MilliSeconds (2));
  senderApps.Stop (MilliSeconds (30));

  Ptr<OutputStreamWrapper> routingStream =
    Create<OutputStreamWrapper> ("optical-ring-failure-demo.routes", std::ios::out);
  Ipv4GlobalRoutingHelper routingHelper;
  routingHelper.PrintRoutingTableAllAt (MilliSeconds (8), routingStream);
  routingHelper.PrintRoutingTableAllAt (MilliSeconds (12), routingStream);
  routingHelper.PrintRoutingTableAllAt (MilliSeconds (18), routingStream);

  NS_LOG_UNCOND ("[Demo] Initial ring: " << orchestrator->DescribeCurrentRing ());
  NS_LOG_UNCOND ("[Demo] Route snapshots will be written to optical-ring-failure-demo.routes");

  Simulator::Schedule (MilliSeconds (10),
                       &ReconfigOrchestrator::InjectNodeFault,
                       orchestrator,
                       1);

  Simulator::Stop (MilliSeconds (35));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
