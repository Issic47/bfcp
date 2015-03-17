set(server_soap_test_SRCS
  main.cpp
  soapBFCPServiceProxy.cpp
  soapC.cpp
  stdsoap2.cpp
  )

add_executable(server_soap_test ${server_soap_test_SRCS})
target_link_libraries(server_soap_test bfcp)

