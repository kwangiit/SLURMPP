/* Generated by the protocol buffer compiler.  DO NOT EDIT! */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C_NO_DEPRECATED
#define PROTOBUF_C_NO_DEPRECATED
#endif

#include "kvstr.pb-c.h"
void   kvstr__init
                     (KVStr         *message)
{
  static KVStr init_value = KVSTR__INIT;
  *message = init_value;
}
size_t kvstr__get_packed_size
                     (const KVStr *message)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &kvstr__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t kvstr__pack
                     (const KVStr *message,
                      uint8_t       *out)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &kvstr__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t kvstr__pack_to_buffer
                     (const KVStr *message,
                      ProtobufCBuffer *buffer)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &kvstr__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
KVStr *
       kvstr__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (KVStr *)
     protobuf_c_message_unpack (&kvstr__descriptor,
                                allocator, len, data);
}
void   kvstr__free_unpacked
                     (KVStr *message,
                      ProtobufCAllocator *allocator)
{
  PROTOBUF_C_ASSERT (message->base.descriptor == &kvstr__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor kvstr__field_descriptors[3] =
{
  {
    "job_id",
    1,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_INT32,
    PROTOBUF_C_OFFSETOF(KVStr, has_job_id),
    PROTOBUF_C_OFFSETOF(KVStr, job_id),
    NULL,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "num_item",
    2,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_INT32,
    PROTOBUF_C_OFFSETOF(KVStr, has_num_item),
    PROTOBUF_C_OFFSETOF(KVStr, num_item),
    NULL,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "name",
    3,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_STRING,
    PROTOBUF_C_OFFSETOF(KVStr, n_name),
    PROTOBUF_C_OFFSETOF(KVStr, name),
    NULL,
    NULL,
    0,            /* packed */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned kvstr__field_indices_by_name[] = {
  0,   /* field[0] = job_id */
  2,   /* field[2] = name */
  1,   /* field[1] = num_item */
};
static const ProtobufCIntRange kvstr__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 3 }
};
const ProtobufCMessageDescriptor kvstr__descriptor =
{
  PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC,
  "KVStr",
  "KVStr",
  "KVStr",
  "",
  sizeof(KVStr),
  3,
  kvstr__field_descriptors,
  kvstr__field_indices_by_name,
  1,  kvstr__number_ranges,
  (ProtobufCMessageInit) kvstr__init,
  NULL,NULL,NULL    /* reserved[123] */
};