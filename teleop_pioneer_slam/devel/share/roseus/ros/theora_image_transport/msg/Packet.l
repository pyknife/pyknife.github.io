;; Auto-generated. Do not edit!


(when (boundp 'theora_image_transport::Packet)
  (if (not (find-package "THEORA_IMAGE_TRANSPORT"))
    (make-package "THEORA_IMAGE_TRANSPORT"))
  (shadow 'Packet (find-package "THEORA_IMAGE_TRANSPORT")))
(unless (find-package "THEORA_IMAGE_TRANSPORT::PACKET")
  (make-package "THEORA_IMAGE_TRANSPORT::PACKET"))

(in-package "ROS")
;;//! \htmlinclude Packet.msg.html
(if (not (find-package "STD_MSGS"))
  (ros::roseus-add-msgs "std_msgs"))


(defclass theora_image_transport::Packet
  :super ros::object
  :slots (_header _data _b_o_s _e_o_s _granulepos _packetno ))

(defmethod theora_image_transport::Packet
  (:init
   (&key
    ((:header __header) (instance std_msgs::Header :init))
    ((:data __data) (make-array 0 :initial-element 0 :element-type :char))
    ((:b_o_s __b_o_s) 0)
    ((:e_o_s __e_o_s) 0)
    ((:granulepos __granulepos) 0)
    ((:packetno __packetno) 0)
    )
   (send-super :init)
   (setq _header __header)
   (setq _data __data)
   (setq _b_o_s (round __b_o_s))
   (setq _e_o_s (round __e_o_s))
   (setq _granulepos (round __granulepos))
   (setq _packetno (round __packetno))
   self)
  (:header
   (&rest __header)
   (if (keywordp (car __header))
       (send* _header __header)
     (progn
       (if __header (setq _header (car __header)))
       _header)))
  (:data
   (&optional __data)
   (if __data (setq _data __data)) _data)
  (:b_o_s
   (&optional __b_o_s)
   (if __b_o_s (setq _b_o_s __b_o_s)) _b_o_s)
  (:e_o_s
   (&optional __e_o_s)
   (if __e_o_s (setq _e_o_s __e_o_s)) _e_o_s)
  (:granulepos
   (&optional __granulepos)
   (if __granulepos (setq _granulepos __granulepos)) _granulepos)
  (:packetno
   (&optional __packetno)
   (if __packetno (setq _packetno __packetno)) _packetno)
  (:serialization-length
   ()
   (+
    ;; std_msgs/Header _header
    (send _header :serialization-length)
    ;; uint8[] _data
    (* 1    (length _data)) 4
    ;; int32 _b_o_s
    4
    ;; int32 _e_o_s
    4
    ;; int64 _granulepos
    8
    ;; int64 _packetno
    8
    ))
  (:serialize
   (&optional strm)
   (let ((s (if strm strm
              (make-string-output-stream (send self :serialization-length)))))
     ;; std_msgs/Header _header
       (send _header :serialize s)
     ;; uint8[] _data
     (write-long (length _data) s)
     (princ _data s)
     ;; int32 _b_o_s
       (write-long _b_o_s s)
     ;; int32 _e_o_s
       (write-long _e_o_s s)
     ;; int64 _granulepos
#+(or :alpha :irix6 :x86_64)
       (progn (sys::poke _granulepos (send s :buffer) (send s :count) :long) (incf (stream-count s) 8))
#-(or :alpha :irix6 :x86_64)
       (cond ((and (class _granulepos) (= (length (_granulepos . bv)) 2)) ;; bignum
              (write-long (ash (elt (_granulepos . bv) 0) 0) s)
              (write-long (ash (elt (_granulepos . bv) 1) -1) s))
             ((and (class _granulepos) (= (length (_granulepos . bv)) 1)) ;; big1
              (write-long (elt (_granulepos . bv) 0) s)
              (write-long (if (>= _granulepos 0) 0 #xffffffff) s))
             (t                                         ;; integer
              (write-long _granulepos s)(write-long (if (>= _granulepos 0) 0 #xffffffff) s)))
     ;; int64 _packetno
#+(or :alpha :irix6 :x86_64)
       (progn (sys::poke _packetno (send s :buffer) (send s :count) :long) (incf (stream-count s) 8))
#-(or :alpha :irix6 :x86_64)
       (cond ((and (class _packetno) (= (length (_packetno . bv)) 2)) ;; bignum
              (write-long (ash (elt (_packetno . bv) 0) 0) s)
              (write-long (ash (elt (_packetno . bv) 1) -1) s))
             ((and (class _packetno) (= (length (_packetno . bv)) 1)) ;; big1
              (write-long (elt (_packetno . bv) 0) s)
              (write-long (if (>= _packetno 0) 0 #xffffffff) s))
             (t                                         ;; integer
              (write-long _packetno s)(write-long (if (>= _packetno 0) 0 #xffffffff) s)))
     ;;
     (if (null strm) (get-output-stream-string s))))
  (:deserialize
   (buf &optional (ptr- 0))
   ;; std_msgs/Header _header
     (send _header :deserialize buf ptr-) (incf ptr- (send _header :serialization-length))
   ;; uint8[] _data
   (let ((n (sys::peek buf ptr- :integer))) (incf ptr- 4)
     (setq _data (make-array n :element-type :char))
     (replace _data buf :start2 ptr-) (incf ptr- n))
   ;; int32 _b_o_s
     (setq _b_o_s (sys::peek buf ptr- :integer)) (incf ptr- 4)
   ;; int32 _e_o_s
     (setq _e_o_s (sys::peek buf ptr- :integer)) (incf ptr- 4)
   ;; int64 _granulepos
#+(or :alpha :irix6 :x86_64)
      (setf _granulepos (prog1 (sys::peek buf ptr- :long) (incf ptr- 8)))
#-(or :alpha :irix6 :x86_64)
      (setf _granulepos (let ((b0 (prog1 (sys::peek buf ptr- :integer) (incf ptr- 4)))
                  (b1 (prog1 (sys::peek buf ptr- :integer) (incf ptr- 4))))
              (cond ((= b1 -1) b0)
                     ((and (= b1  0)
                           (<= lisp::most-negative-fixnum b0 lisp::most-positive-fixnum))
                      b0)
                    ((= b1  0) (make-instance bignum :size 1 :bv (integer-vector b0)))
                    (t (make-instance bignum :size 2 :bv (integer-vector b0 (ash b1 1)))))))
   ;; int64 _packetno
#+(or :alpha :irix6 :x86_64)
      (setf _packetno (prog1 (sys::peek buf ptr- :long) (incf ptr- 8)))
#-(or :alpha :irix6 :x86_64)
      (setf _packetno (let ((b0 (prog1 (sys::peek buf ptr- :integer) (incf ptr- 4)))
                  (b1 (prog1 (sys::peek buf ptr- :integer) (incf ptr- 4))))
              (cond ((= b1 -1) b0)
                     ((and (= b1  0)
                           (<= lisp::most-negative-fixnum b0 lisp::most-positive-fixnum))
                      b0)
                    ((= b1  0) (make-instance bignum :size 1 :bv (integer-vector b0)))
                    (t (make-instance bignum :size 2 :bv (integer-vector b0 (ash b1 1)))))))
   ;;
   self)
  )

(setf (get theora_image_transport::Packet :md5sum-) "33ac4e14a7cff32e7e0d65f18bb410f3")
(setf (get theora_image_transport::Packet :datatype-) "theora_image_transport/Packet")
(setf (get theora_image_transport::Packet :definition-)
      "# ROS message adaptation of the ogg_packet struct from libogg,
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

")



(provide :theora_image_transport/Packet "33ac4e14a7cff32e7e0d65f18bb410f3")


