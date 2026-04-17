build_lib(
  LIBNAME optical-reconfig
  SOURCE_FILES
    model/fault-manager.cc
    model/communication-topology-manager.cc
    model/optical-circuit-manager.cc
    model/ring-bypass-policy.cc
    model/reconfig-orchestrator.cc
    helper/optical-reconfig-helper.cc
  HEADER_FILES
    model/optical-types.h
    model/fault-manager.h
    model/communication-topology-manager.h
    model/optical-circuit-manager.h
    model/reconfig-policy.h
    model/ring-bypass-policy.h
    model/reconfig-orchestrator.h
    helper/optical-reconfig-helper.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libinternet}
  TEST_SOURCES
    test/optical-reconfig-test-suite.cc
)

build_lib_example(
  NAME optical-ring-failure-demo
  SOURCE_FILES examples/optical-ring-failure-demo.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libapplications}
    ${libpoint-to-point}
    ${liboptical-reconfig}
)
