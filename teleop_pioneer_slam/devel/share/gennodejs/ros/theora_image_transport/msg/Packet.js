// Auto-generated. Do not edit!

// (in-package theora_image_transport.msg)


"use strict";

const _serializer = _ros_msg_utils.Serialize;
const _arraySerializer = _serializer.Array;
const _deserializer = _ros_msg_utils.Deserialize;
const _arrayDeserializer = _deserializer.Array;
const _finder = _ros_msg_utils.Find;
const _getByteLength = _ros_msg_utils.getByteLength;
let std_msgs = _finder('std_msgs');

//-----------------------------------------------------------

class Packet {
  constructor(initObj={}) {
    if (initObj === null) {
      // initObj === null is a special case for deserialization where we don't initialize fields
      this.header = null;
      this.data = null;
      this.b_o_s = null;
      this.e_o_s = null;
      this.granulepos = null;
      this.packetno = null;
    }
    else {
      if (initObj.hasOwnProperty('header')) {
        this.header = initObj.header
      }
      else {
        this.header = new std_msgs.msg.Header();
      }
      if (initObj.hasOwnProperty('data')) {
        this.data = initObj.data
      }
      else {
        this.data = [];
      }
      if (initObj.hasOwnProperty('b_o_s')) {
        this.b_o_s = initObj.b_o_s
      }
      else {
        this.b_o_s = 0;
      }
      if (initObj.hasOwnProperty('e_o_s')) {
        this.e_o_s = initObj.e_o_s
      }
      else {
        this.e_o_s = 0;
      }
      if (initObj.hasOwnProperty('granulepos')) {
        this.granulepos = initObj.granulepos
      }
      else {
        this.granulepos = 0;
      }
      if (initObj.hasOwnProperty('packetno')) {
        this.packetno = initObj.packetno
      }
      else {
        this.packetno = 0;
      }
    }
  }

  static serialize(obj, buffer, bufferOffset) {
    // Serializes a message object of type Packet
    // Serialize message field [header]
    bufferOffset = std_msgs.msg.Header.serialize(obj.header, buffer, bufferOffset);
    // Serialize message field [data]
    bufferOffset = _arraySerializer.uint8(obj.data, buffer, bufferOffset, null);
    // Serialize message field [b_o_s]
    bufferOffset = _serializer.int32(obj.b_o_s, buffer, bufferOffset);
    // Serialize message field [e_o_s]
    bufferOffset = _serializer.int32(obj.e_o_s, buffer, bufferOffset);
    // Serialize message field [granulepos]
    bufferOffset = _serializer.int64(obj.granulepos, buffer, bufferOffset);
    // Serialize message field [packetno]
    bufferOffset = _serializer.int64(obj.packetno, buffer, bufferOffset);
    return bufferOffset;
  }

  static deserialize(buffer, bufferOffset=[0]) {
    //deserializes a message object of type Packet
    let len;
    let data = new Packet(null);
    // Deserialize message field [header]
    data.header = std_msgs.msg.Header.deserialize(buffer, bufferOffset);
    // Deserialize message field [data]
    data.data = _arrayDeserializer.uint8(buffer, bufferOffset, null)
    // Deserialize message field [b_o_s]
    data.b_o_s = _deserializer.int32(buffer, bufferOffset);
    // Deserialize message field [e_o_s]
    data.e_o_s = _deserializer.int32(buffer, bufferOffset);
    // Deserialize message field [granulepos]
    data.granulepos = _deserializer.int64(buffer, bufferOffset);
    // Deserialize message field [packetno]
    data.packetno = _deserializer.int64(buffer, bufferOffset);
    return data;
  }

  static getMessageSize(object) {
    let length = 0;
    length += std_msgs.msg.Header.getMessageSize(object.header);
    length += object.data.length;
    return length + 28;
  }

  static datatype() {
    // Returns string type for a message object
    return 'theora_image_transport/Packet';
  }

  static md5sum() {
    //Returns md5sum for a message object
    return '33ac4e14a7cff32e7e0d65f18bb410f3';
  }

  static messageDefinition() {
    // Returns full string definition for message
    return `
    # ROS message adaptation of the ogg_packet struct from libogg,
    # see http://www.xiph.org/ogg/doc/libogg/ogg_packet.html.
    
    Header header     # Original sensor_msgs/Image header
    uint8[] data      # Raw Theora packet data (combines packet and bytes fields from ogg_packet)
    int32 b_o_s       # Flag indicating whether this packet begins a logical bitstream
    int32 e_o_s       # Flag indicating whether this packet ends a bitstream
    int64 granulepos  # A number indicating the position of this packet in the decoded data
    int64 packetno    # Sequential number of this packet in the ogg bitstream
    
    ================================================================================
    MSG: std_msgs/Header
    # Standard metadata for higher-level stamped data types.
    # This is generally used to communicate timestamped data 
    # in a particular coordinate frame.
    # 
    # sequence ID: consecutively increasing ID 
    uint32 seq
    #Two-integer timestamp that is expressed as:
    # * stamp.sec: seconds (stamp_secs) since epoch (in Python the variable is called 'secs')
    # * stamp.nsec: nanoseconds since stamp_secs (in Python the variable is called 'nsecs')
    # time-handling sugar is provided by the client library
    time stamp
    #Frame this data is associated with
    # 0: no frame
    # 1: global frame
    string frame_id
    
    `;
  }

  static Resolve(msg) {
    // deep-construct a valid message object instance of whatever was passed in
    if (typeof msg !== 'object' || msg === null) {
      msg = {};
    }
    const resolved = new Packet(null);
    if (msg.header !== undefined) {
      resolved.header = std_msgs.msg.Header.Resolve(msg.header)
    }
    else {
      resolved.header = new std_msgs.msg.Header()
    }

    if (msg.data !== undefined) {
      resolved.data = msg.data;
    }
    else {
      resolved.data = []
    }

    if (msg.b_o_s !== undefined) {
      resolved.b_o_s = msg.b_o_s;
    }
    else {
      resolved.b_o_s = 0
    }

    if (msg.e_o_s !== undefined) {
      resolved.e_o_s = msg.e_o_s;
    }
    else {
      resolved.e_o_s = 0
    }

    if (msg.granulepos !== undefined) {
      resolved.granulepos = msg.granulepos;
    }
    else {
      resolved.granulepos = 0
    }

    if (msg.packetno !== undefined) {
      resolved.packetno = msg.packetno;
    }
    else {
      resolved.packetno = 0
    }

    return resolved;
    }
};

module.exports = Packet;
