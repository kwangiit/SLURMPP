/* Generated by the protocol buffer compiler.  DO NOT EDIT! */

#ifndef PROTOBUF_C_kvstr_2eproto__INCLUDED
#define PROTOBUF_C_kvstr_2eproto__INCLUDED

#include <google/protobuf-c/protobuf-c.h>

PROTOBUF_C_BEGIN_DECLS


typedef struct _KVStr KVStr;


/* --- enums --- */


/* --- messages --- */

struct  _KVStr
{
  ProtobufCMessage base;
  protobuf_c_boolean has_job_id;
  int32_t job_id;
  protobuf_c_boolean has_num_item;
  int32_t num_item;
  size_t n_name;
  char **name;
};
#define KVSTR__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&kvstr__descriptor) \
    , 0,0, 0,0, 0,NULL }


/* KVStr methods */
void   kvstr__init
                     (KVStr         *message);
size_t kvstr__get_packed_size
                     (const KVStr   *message);
size_t kvstr__pack
                     (const KVStr   *message,
                      uint8_t             *out);
size_t kvstr__pack_to_buffer
                     (const KVStr   *message,
                      ProtobufCBuffer     *buffer);
KVStr *
       kvstr__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   kvstr__free_unpacked
                     (KVStr *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*KVStr_Closure)
                 (const KVStr *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor kvstr__descriptor;

PROTOBUF_C_END_DECLS


#endif  /* PROTOBUF_kvstr_2eproto__INCLUDED */
