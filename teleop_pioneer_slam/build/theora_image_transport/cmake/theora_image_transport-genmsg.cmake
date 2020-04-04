# generated from genmsg/cmake/pkg-genmsg.cmake.em

message(STATUS "theora_image_transport: 1 messages, 0 services")

set(MSG_I_FLAGS "-Itheora_image_transport:/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg;-Istd_msgs:/opt/ros/kinetic/share/std_msgs/cmake/../msg")

# Find all generators
find_package(gencpp REQUIRED)
find_package(geneus REQUIRED)
find_package(genlisp REQUIRED)
find_package(gennodejs REQUIRED)
find_package(genpy REQUIRED)

add_custom_target(theora_image_transport_generate_messages ALL)

# verify that message/service dependencies have not changed since configure



get_filename_component(_filename "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg" NAME_WE)
add_custom_target(_theora_image_transport_generate_messages_check_deps_${_filename}
  COMMAND ${CATKIN_ENV} ${PYTHON_EXECUTABLE} ${GENMSG_CHECK_DEPS_SCRIPT} "theora_image_transport" "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg" "std_msgs/Header"
)

#
#  langs = gencpp;geneus;genlisp;gennodejs;genpy
#

### Section generating for lang: gencpp
### Generating Messages
_generate_msg_cpp(theora_image_transport
  "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg"
  "${MSG_I_FLAGS}"
  "/opt/ros/kinetic/share/std_msgs/cmake/../msg/Header.msg"
  ${CATKIN_DEVEL_PREFIX}/${gencpp_INSTALL_DIR}/theora_image_transport
)

### Generating Services

### Generating Module File
_generate_module_cpp(theora_image_transport
  ${CATKIN_DEVEL_PREFIX}/${gencpp_INSTALL_DIR}/theora_image_transport
  "${ALL_GEN_OUTPUT_FILES_cpp}"
)

add_custom_target(theora_image_transport_generate_messages_cpp
  DEPENDS ${ALL_GEN_OUTPUT_FILES_cpp}
)
add_dependencies(theora_image_transport_generate_messages theora_image_transport_generate_messages_cpp)

# add dependencies to all check dependencies targets
get_filename_component(_filename "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg" NAME_WE)
add_dependencies(theora_image_transport_generate_messages_cpp _theora_image_transport_generate_messages_check_deps_${_filename})

# target for backward compatibility
add_custom_target(theora_image_transport_gencpp)
add_dependencies(theora_image_transport_gencpp theora_image_transport_generate_messages_cpp)

# register target for catkin_package(EXPORTED_TARGETS)
list(APPEND ${PROJECT_NAME}_EXPORTED_TARGETS theora_image_transport_generate_messages_cpp)

### Section generating for lang: geneus
### Generating Messages
_generate_msg_eus(theora_image_transport
  "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg"
  "${MSG_I_FLAGS}"
  "/opt/ros/kinetic/share/std_msgs/cmake/../msg/Header.msg"
  ${CATKIN_DEVEL_PREFIX}/${geneus_INSTALL_DIR}/theora_image_transport
)

### Generating Services

### Generating Module File
_generate_module_eus(theora_image_transport
  ${CATKIN_DEVEL_PREFIX}/${geneus_INSTALL_DIR}/theora_image_transport
  "${ALL_GEN_OUTPUT_FILES_eus}"
)

add_custom_target(theora_image_transport_generate_messages_eus
  DEPENDS ${ALL_GEN_OUTPUT_FILES_eus}
)
add_dependencies(theora_image_transport_generate_messages theora_image_transport_generate_messages_eus)

# add dependencies to all check dependencies targets
get_filename_component(_filename "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg" NAME_WE)
add_dependencies(theora_image_transport_generate_messages_eus _theora_image_transport_generate_messages_check_deps_${_filename})

# target for backward compatibility
add_custom_target(theora_image_transport_geneus)
add_dependencies(theora_image_transport_geneus theora_image_transport_generate_messages_eus)

# register target for catkin_package(EXPORTED_TARGETS)
list(APPEND ${PROJECT_NAME}_EXPORTED_TARGETS theora_image_transport_generate_messages_eus)

### Section generating for lang: genlisp
### Generating Messages
_generate_msg_lisp(theora_image_transport
  "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg"
  "${MSG_I_FLAGS}"
  "/opt/ros/kinetic/share/std_msgs/cmake/../msg/Header.msg"
  ${CATKIN_DEVEL_PREFIX}/${genlisp_INSTALL_DIR}/theora_image_transport
)

### Generating Services

### Generating Module File
_generate_module_lisp(theora_image_transport
  ${CATKIN_DEVEL_PREFIX}/${genlisp_INSTALL_DIR}/theora_image_transport
  "${ALL_GEN_OUTPUT_FILES_lisp}"
)

add_custom_target(theora_image_transport_generate_messages_lisp
  DEPENDS ${ALL_GEN_OUTPUT_FILES_lisp}
)
add_dependencies(theora_image_transport_generate_messages theora_image_transport_generate_messages_lisp)

# add dependencies to all check dependencies targets
get_filename_component(_filename "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg" NAME_WE)
add_dependencies(theora_image_transport_generate_messages_lisp _theora_image_transport_generate_messages_check_deps_${_filename})

# target for backward compatibility
add_custom_target(theora_image_transport_genlisp)
add_dependencies(theora_image_transport_genlisp theora_image_transport_generate_messages_lisp)

# register target for catkin_package(EXPORTED_TARGETS)
list(APPEND ${PROJECT_NAME}_EXPORTED_TARGETS theora_image_transport_generate_messages_lisp)

### Section generating for lang: gennodejs
### Generating Messages
_generate_msg_nodejs(theora_image_transport
  "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg"
  "${MSG_I_FLAGS}"
  "/opt/ros/kinetic/share/std_msgs/cmake/../msg/Header.msg"
  ${CATKIN_DEVEL_PREFIX}/${gennodejs_INSTALL_DIR}/theora_image_transport
)

### Generating Services

### Generating Module File
_generate_module_nodejs(theora_image_transport
  ${CATKIN_DEVEL_PREFIX}/${gennodejs_INSTALL_DIR}/theora_image_transport
  "${ALL_GEN_OUTPUT_FILES_nodejs}"
)

add_custom_target(theora_image_transport_generate_messages_nodejs
  DEPENDS ${ALL_GEN_OUTPUT_FILES_nodejs}
)
add_dependencies(theora_image_transport_generate_messages theora_image_transport_generate_messages_nodejs)

# add dependencies to all check dependencies targets
get_filename_component(_filename "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg" NAME_WE)
add_dependencies(theora_image_transport_generate_messages_nodejs _theora_image_transport_generate_messages_check_deps_${_filename})

# target for backward compatibility
add_custom_target(theora_image_transport_gennodejs)
add_dependencies(theora_image_transport_gennodejs theora_image_transport_generate_messages_nodejs)

# register target for catkin_package(EXPORTED_TARGETS)
list(APPEND ${PROJECT_NAME}_EXPORTED_TARGETS theora_image_transport_generate_messages_nodejs)

### Section generating for lang: genpy
### Generating Messages
_generate_msg_py(theora_image_transport
  "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg"
  "${MSG_I_FLAGS}"
  "/opt/ros/kinetic/share/std_msgs/cmake/../msg/Header.msg"
  ${CATKIN_DEVEL_PREFIX}/${genpy_INSTALL_DIR}/theora_image_transport
)

### Generating Services

### Generating Module File
_generate_module_py(theora_image_transport
  ${CATKIN_DEVEL_PREFIX}/${genpy_INSTALL_DIR}/theora_image_transport
  "${ALL_GEN_OUTPUT_FILES_py}"
)

add_custom_target(theora_image_transport_generate_messages_py
  DEPENDS ${ALL_GEN_OUTPUT_FILES_py}
)
add_dependencies(theora_image_transport_generate_messages theora_image_transport_generate_messages_py)

# add dependencies to all check dependencies targets
get_filename_component(_filename "/home/l/teleop_pioneer_slam/src/image_transport_plugins/theora_image_transport/msg/Packet.msg" NAME_WE)
add_dependencies(theora_image_transport_generate_messages_py _theora_image_transport_generate_messages_check_deps_${_filename})

# target for backward compatibility
add_custom_target(theora_image_transport_genpy)
add_dependencies(theora_image_transport_genpy theora_image_transport_generate_messages_py)

# register target for catkin_package(EXPORTED_TARGETS)
list(APPEND ${PROJECT_NAME}_EXPORTED_TARGETS theora_image_transport_generate_messages_py)



if(gencpp_INSTALL_DIR AND EXISTS ${CATKIN_DEVEL_PREFIX}/${gencpp_INSTALL_DIR}/theora_image_transport)
  # install generated code
  install(
    DIRECTORY ${CATKIN_DEVEL_PREFIX}/${gencpp_INSTALL_DIR}/theora_image_transport
    DESTINATION ${gencpp_INSTALL_DIR}
  )
endif()
if(TARGET std_msgs_generate_messages_cpp)
  add_dependencies(theora_image_transport_generate_messages_cpp std_msgs_generate_messages_cpp)
endif()

if(geneus_INSTALL_DIR AND EXISTS ${CATKIN_DEVEL_PREFIX}/${geneus_INSTALL_DIR}/theora_image_transport)
  # install generated code
  install(
    DIRECTORY ${CATKIN_DEVEL_PREFIX}/${geneus_INSTALL_DIR}/theora_image_transport
    DESTINATION ${geneus_INSTALL_DIR}
  )
endif()
if(TARGET std_msgs_generate_messages_eus)
  add_dependencies(theora_image_transport_generate_messages_eus std_msgs_generate_messages_eus)
endif()

if(genlisp_INSTALL_DIR AND EXISTS ${CATKIN_DEVEL_PREFIX}/${genlisp_INSTALL_DIR}/theora_image_transport)
  # install generated code
  install(
    DIRECTORY ${CATKIN_DEVEL_PREFIX}/${genlisp_INSTALL_DIR}/theora_image_transport
    DESTINATION ${genlisp_INSTALL_DIR}
  )
endif()
if(TARGET std_msgs_generate_messages_lisp)
  add_dependencies(theora_image_transport_generate_messages_lisp std_msgs_generate_messages_lisp)
endif()

if(gennodejs_INSTALL_DIR AND EXISTS ${CATKIN_DEVEL_PREFIX}/${gennodejs_INSTALL_DIR}/theora_image_transport)
  # install generated code
  install(
    DIRECTORY ${CATKIN_DEVEL_PREFIX}/${gennodejs_INSTALL_DIR}/theora_image_transport
    DESTINATION ${gennodejs_INSTALL_DIR}
  )
endif()
if(TARGET std_msgs_generate_messages_nodejs)
  add_dependencies(theora_image_transport_generate_messages_nodejs std_msgs_generate_messages_nodejs)
endif()

if(genpy_INSTALL_DIR AND EXISTS ${CATKIN_DEVEL_PREFIX}/${genpy_INSTALL_DIR}/theora_image_transport)
  install(CODE "execute_process(COMMAND \"/usr/bin/python\" -m compileall \"${CATKIN_DEVEL_PREFIX}/${genpy_INSTALL_DIR}/theora_image_transport\")")
  # install generated code
  install(
    DIRECTORY ${CATKIN_DEVEL_PREFIX}/${genpy_INSTALL_DIR}/theora_image_transport
    DESTINATION ${genpy_INSTALL_DIR}
  )
endif()
if(TARGET std_msgs_generate_messages_py)
  add_dependencies(theora_image_transport_generate_messages_py std_msgs_generate_messages_py)
endif()
