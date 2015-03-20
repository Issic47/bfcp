set(bfcp_server_soap_SRCS
  basename.cpp
  main.cpp
  soapBFCPServiceService.cpp
  soapC.cpp
  stdsoap2.cpp
  BfcpService.cpp
  )

add_executable(bfcp_server_soap ${bfcp_server_soap_SRCS})
target_link_libraries(bfcp_server_soap bfcp)

