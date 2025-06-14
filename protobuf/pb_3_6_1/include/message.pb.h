// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: message.proto

#ifndef PROTOBUF_INCLUDED_message_2eproto
#define PROTOBUF_INCLUDED_message_2eproto

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 3006001
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 3006001 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_table_driven.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/inlined_string_field.h>
#include <google/protobuf/metadata.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>  // IWYU pragma: export
#include <google/protobuf/extension_set.h>  // IWYU pragma: export
#include <google/protobuf/unknown_field_set.h>
// @@protoc_insertion_point(includes)
#define PROTOBUF_INTERNAL_EXPORT_protobuf_message_2eproto 

namespace protobuf_message_2eproto {
// Internal implementation detail -- do not use these members.
struct TableStruct {
  static const ::google::protobuf::internal::ParseTableField entries[];
  static const ::google::protobuf::internal::AuxillaryParseTableField aux[];
  static const ::google::protobuf::internal::ParseTable schema[1];
  static const ::google::protobuf::internal::FieldMetadata field_metadata[];
  static const ::google::protobuf::internal::SerializationTable serialization_table[];
  static const ::google::protobuf::uint32 offsets[];
};
void AddDescriptors();
}  // namespace protobuf_message_2eproto
namespace benchmark {
class TestMessage;
class TestMessageDefaultTypeInternal;
extern TestMessageDefaultTypeInternal _TestMessage_default_instance_;
}  // namespace benchmark
namespace google {
namespace protobuf {
template<> ::benchmark::TestMessage* Arena::CreateMaybeMessage<::benchmark::TestMessage>(Arena*);
}  // namespace protobuf
}  // namespace google
namespace benchmark {

// ===================================================================

class TestMessage : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:benchmark.TestMessage) */ {
 public:
  TestMessage();
  virtual ~TestMessage();

  TestMessage(const TestMessage& from);

  inline TestMessage& operator=(const TestMessage& from) {
    CopyFrom(from);
    return *this;
  }
  #if LANG_CXX11
  TestMessage(TestMessage&& from) noexcept
    : TestMessage() {
    *this = ::std::move(from);
  }

  inline TestMessage& operator=(TestMessage&& from) noexcept {
    if (GetArenaNoVirtual() == from.GetArenaNoVirtual()) {
      if (this != &from) InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }
  #endif
  static const ::google::protobuf::Descriptor* descriptor();
  static const TestMessage& default_instance();

  static void InitAsDefaultInstance();  // FOR INTERNAL USE ONLY
  static inline const TestMessage* internal_default_instance() {
    return reinterpret_cast<const TestMessage*>(
               &_TestMessage_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    0;

  void Swap(TestMessage* other);
  friend void swap(TestMessage& a, TestMessage& b) {
    a.Swap(&b);
  }

  // implements Message ----------------------------------------------

  inline TestMessage* New() const final {
    return CreateMaybeMessage<TestMessage>(NULL);
  }

  TestMessage* New(::google::protobuf::Arena* arena) const final {
    return CreateMaybeMessage<TestMessage>(arena);
  }
  void CopyFrom(const ::google::protobuf::Message& from) final;
  void MergeFrom(const ::google::protobuf::Message& from) final;
  void CopyFrom(const TestMessage& from);
  void MergeFrom(const TestMessage& from);
  void Clear() final;
  bool IsInitialized() const final;

  size_t ByteSizeLong() const final;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input) final;
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const final;
  ::google::protobuf::uint8* InternalSerializeWithCachedSizesToArray(
      bool deterministic, ::google::protobuf::uint8* target) const final;
  int GetCachedSize() const final { return _cached_size_.Get(); }

  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const final;
  void InternalSwap(TestMessage* other);
  private:
  inline ::google::protobuf::Arena* GetArenaNoVirtual() const {
    return NULL;
  }
  inline void* MaybeArenaPtr() const {
    return NULL;
  }
  public:

  ::google::protobuf::Metadata GetMetadata() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // repeated int32 values = 3;
  int values_size() const;
  void clear_values();
  static const int kValuesFieldNumber = 3;
  ::google::protobuf::int32 values(int index) const;
  void set_values(int index, ::google::protobuf::int32 value);
  void add_values(::google::protobuf::int32 value);
  const ::google::protobuf::RepeatedField< ::google::protobuf::int32 >&
      values() const;
  ::google::protobuf::RepeatedField< ::google::protobuf::int32 >*
      mutable_values();

  // string name = 2;
  void clear_name();
  static const int kNameFieldNumber = 2;
  const ::std::string& name() const;
  void set_name(const ::std::string& value);
  #if LANG_CXX11
  void set_name(::std::string&& value);
  #endif
  void set_name(const char* value);
  void set_name(const char* value, size_t size);
  ::std::string* mutable_name();
  ::std::string* release_name();
  void set_allocated_name(::std::string* name);

  // int32 id = 1;
  void clear_id();
  static const int kIdFieldNumber = 1;
  ::google::protobuf::int32 id() const;
  void set_id(::google::protobuf::int32 value);

  // @@protoc_insertion_point(class_scope:benchmark.TestMessage)
 private:

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  ::google::protobuf::RepeatedField< ::google::protobuf::int32 > values_;
  mutable int _values_cached_byte_size_;
  ::google::protobuf::internal::ArenaStringPtr name_;
  ::google::protobuf::int32 id_;
  mutable ::google::protobuf::internal::CachedSize _cached_size_;
  friend struct ::protobuf_message_2eproto::TableStruct;
};
// ===================================================================


// ===================================================================

#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// TestMessage

// int32 id = 1;
inline void TestMessage::clear_id() {
  id_ = 0;
}
inline ::google::protobuf::int32 TestMessage::id() const {
  // @@protoc_insertion_point(field_get:benchmark.TestMessage.id)
  return id_;
}
inline void TestMessage::set_id(::google::protobuf::int32 value) {
  
  id_ = value;
  // @@protoc_insertion_point(field_set:benchmark.TestMessage.id)
}

// string name = 2;
inline void TestMessage::clear_name() {
  name_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline const ::std::string& TestMessage::name() const {
  // @@protoc_insertion_point(field_get:benchmark.TestMessage.name)
  return name_.GetNoArena();
}
inline void TestMessage::set_name(const ::std::string& value) {
  
  name_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:benchmark.TestMessage.name)
}
#if LANG_CXX11
inline void TestMessage::set_name(::std::string&& value) {
  
  name_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:benchmark.TestMessage.name)
}
#endif
inline void TestMessage::set_name(const char* value) {
  GOOGLE_DCHECK(value != NULL);
  
  name_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:benchmark.TestMessage.name)
}
inline void TestMessage::set_name(const char* value, size_t size) {
  
  name_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:benchmark.TestMessage.name)
}
inline ::std::string* TestMessage::mutable_name() {
  
  // @@protoc_insertion_point(field_mutable:benchmark.TestMessage.name)
  return name_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* TestMessage::release_name() {
  // @@protoc_insertion_point(field_release:benchmark.TestMessage.name)
  
  return name_.ReleaseNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void TestMessage::set_allocated_name(::std::string* name) {
  if (name != NULL) {
    
  } else {
    
  }
  name_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), name);
  // @@protoc_insertion_point(field_set_allocated:benchmark.TestMessage.name)
}

// repeated int32 values = 3;
inline int TestMessage::values_size() const {
  return values_.size();
}
inline void TestMessage::clear_values() {
  values_.Clear();
}
inline ::google::protobuf::int32 TestMessage::values(int index) const {
  // @@protoc_insertion_point(field_get:benchmark.TestMessage.values)
  return values_.Get(index);
}
inline void TestMessage::set_values(int index, ::google::protobuf::int32 value) {
  values_.Set(index, value);
  // @@protoc_insertion_point(field_set:benchmark.TestMessage.values)
}
inline void TestMessage::add_values(::google::protobuf::int32 value) {
  values_.Add(value);
  // @@protoc_insertion_point(field_add:benchmark.TestMessage.values)
}
inline const ::google::protobuf::RepeatedField< ::google::protobuf::int32 >&
TestMessage::values() const {
  // @@protoc_insertion_point(field_list:benchmark.TestMessage.values)
  return values_;
}
inline ::google::protobuf::RepeatedField< ::google::protobuf::int32 >*
TestMessage::mutable_values() {
  // @@protoc_insertion_point(field_mutable_list:benchmark.TestMessage.values)
  return &values_;
}

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)

}  // namespace benchmark

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_INCLUDED_message_2eproto
