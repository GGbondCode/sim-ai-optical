#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include "optical-types.h"
#include "ns3/callback.h"
#include "ns3/object.h"

#include <vector>

namespace ns3 {

class FaultManager : public Object
{
public:
  static TypeId GetTypeId (void);

  FaultManager ();
  ~FaultManager () override;

  void SetFaultCallback (Callback<void, FaultEvent> callback);
  void InjectFault (const FaultEvent& event);
  void InjectNodeFault (uint32_t nodeId);

  const std::vector<FaultEvent>& GetFaultEvents () const;

private:
  Callback<void, FaultEvent> m_faultCallback;
  std::vector<FaultEvent> m_faultEvents;
};

} // namespace ns3

#endif // FAULT_MANAGER_H
