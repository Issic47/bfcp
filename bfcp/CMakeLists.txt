set(bfcp_SRCS
  common/bfcp_attr.cpp
  common/bfcp_conn.cpp
  common/bfcp_ctrans.cpp
  common/bfcp_msg_build.cpp
  common/bfcp_msg.cpp
  common/bfcp_param.cpp
  client/base_client.cpp
  server/base_server.cpp
  server/conference.cpp
  server/floor_request_node.cpp
  server/task_queue.cpp
  server/thread_pool.cpp
  server/user.cpp
  )

add_library(bfcp ${bfcp_SRCS})
target_link_libraries(bfcp ${MUDUOX_LIBS} ${LIBUV_LIBRARY} ${LIBRE_LIBRARY} ${TINYXML2_LIBRARY}  pthread rt)

install(TARGETS bfcp DESTINATION lib)

file(GLOB HEADERS "common/*.h")
install(FILES ${HEADERS} DESTINATION include/bfcp/common)

file(GLOB HEADERS "client/*.h")
install(FILES ${HEADERS} DESTINATION include/bfcp/client)

file(GLOB HEADERS "server/*.h")
install(FILES ${HEADERS} DESTINATION include/bfcp/server)
