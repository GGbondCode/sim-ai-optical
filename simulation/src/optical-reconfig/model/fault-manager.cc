#include "fault-manager.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FaultManager");
NS_OBJECT_ENSURE_REGISTERED (FaultManager);

TypeId
FaultManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FaultManager")
    .SetParent<Object> ()
    .SetGroupName ("OpticalReconfig")
    .AddConstructor<FaultManager> ();
  return tid;
}

FaultManager::FaultManager () = default;

FaultManager::~FaultManager () = default;

void
FaultManager::SetFaultCallback (Callback<void, FaultEvent> callback)
{
  m_faultCallback = callback;
}

void
FaultManager::InjectFault (const FaultEvent& event)
{
  m_faultEvents.push_back (event);
  NS_LOG_INFO ("Inject fault type=" << event.type << " node=" << event.node_id);

  if (!m_faultCallback.IsNull ())
    {
      m_faultCallback (event);
    }
}

void
FaultManager::InjectNodeFault (uint32_t nodeId)
{
  FaultEvent event;
  event.type = FaultEvent::NODE;
  event.node_id = nodeId;
  event.time_ns = Simulator::Now ().GetNanoSeconds ();
  InjectFault (event);
}

const std::vector<FaultEvent>&
FaultManager::GetFaultEvents () const
{
  return m_faultEvents;
}

} // namespace ns3
