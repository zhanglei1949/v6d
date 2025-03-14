/** Copyright 2020-2022 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "common/util/protocols.h"

#include <sstream>
#include <unordered_set>

#include "common/util/logging.h"
#include "common/util/uuid.h"
#include "common/util/version.h"

namespace vineyard {

#define CHECK_IPC_ERROR(tree, type)                                      \
  do {                                                                   \
    if (tree.contains("code")) {                                         \
      Status st = Status(static_cast<StatusCode>(tree.value("code", 0)), \
                         tree.value("message", ""));                     \
      if (!st.ok()) {                                                    \
        return st;                                                       \
      }                                                                  \
    }                                                                    \
    RETURN_ON_ASSERT(root["type"] == (type));                            \
  } while (0)

CommandType ParseCommandType(const std::string& str_type) {
  if (str_type == "exit_request") {
    return CommandType::ExitRequest;
  } else if (str_type == "exit_reply") {
    return CommandType::ExitReply;
  } else if (str_type == "register_request") {
    return CommandType::RegisterRequest;
  } else if (str_type == "register_reply") {
    return CommandType::RegisterReply;
  } else if (str_type == "get_data_request") {
    return CommandType::GetDataRequest;
  } else if (str_type == "get_data_reply") {
    return CommandType::GetDataReply;
  } else if (str_type == "create_data_request") {
    return CommandType::CreateDataRequest;
  } else if (str_type == "persist_request") {
    return CommandType::PersistRequest;
  } else if (str_type == "exists_request") {
    return CommandType::ExistsRequest;
  } else if (str_type == "del_data_request") {
    return CommandType::DelDataRequest;
  } else if (str_type == "cluster_meta") {
    return CommandType::ClusterMetaRequest;
  } else if (str_type == "list_data_request") {
    return CommandType::ListDataRequest;
  } else if (str_type == "create_buffer_request") {
    return CommandType::CreateBufferRequest;
  } else if (str_type == "create_disk_buffer_request") {
    return CommandType::CreateDiskBufferRequest;
  } else if (str_type == "get_buffers_request") {
    return CommandType::GetBuffersRequest;
  } else if (str_type == "create_stream_request") {
    return CommandType::CreateStreamRequest;
  } else if (str_type == "get_next_stream_chunk_request") {
    return CommandType::GetNextStreamChunkRequest;
  } else if (str_type == "push_next_stream_chunk_request") {
    return CommandType::PushNextStreamChunkRequest;
  } else if (str_type == "pull_next_stream_chunk_request") {
    return CommandType::PullNextStreamChunkRequest;
  } else if (str_type == "stop_stream_request") {
    return CommandType::StopStreamRequest;
  } else if (str_type == "put_name_request") {
    return CommandType::PutNameRequest;
  } else if (str_type == "get_name_request") {
    return CommandType::GetNameRequest;
  } else if (str_type == "drop_name_request") {
    return CommandType::DropNameRequest;
  } else if (str_type == "if_persist_request") {
    return CommandType::IfPersistRequest;
  } else if (str_type == "instance_status_request") {
    return CommandType::InstanceStatusRequest;
  } else if (str_type == "shallow_copy_request") {
    return CommandType::ShallowCopyRequest;
  } else if (str_type == "open_stream_request") {
    return CommandType::OpenStreamRequest;
  } else if (str_type == "migrate_object_request") {
    return CommandType::MigrateObjectRequest;
  } else if (str_type == "create_remote_buffer_request") {
    return CommandType::CreateRemoteBufferRequest;
  } else if (str_type == "get_remote_buffers_request") {
    return CommandType::GetRemoteBuffersRequest;
  } else if (str_type == "drop_buffer_request") {
    return CommandType::DropBufferRequest;
  } else if (str_type == "make_arena_request") {
    return CommandType::MakeArenaRequest;
  } else if (str_type == "finalize_arena_request") {
    return CommandType::FinalizeArenaRequest;
  } else if (str_type == "clear_request") {
    return CommandType::ClearRequest;
  } else if (str_type == "debug_command") {
    return CommandType::DebugCommand;
  } else if (str_type == "new_session_request") {
    return CommandType::NewSessionRequest;
  } else if (str_type == "new_session_reply") {
    return CommandType::NewSessionReply;
  } else if (str_type == "delete_session_request") {
    return CommandType::DeleteSessionRequest;
  } else if (str_type == "delete_session_reply") {
    return CommandType::DeleteSessionReply;
  } else if (str_type == "create_buffer_by_plasma_request") {
    return CommandType::CreateBufferByPlasmaRequest;
  } else if (str_type == "get_buffers_by_plasma_request") {
    return CommandType::GetBuffersByPlasmaRequest;
  } else if (str_type == "seal_request") {
    return CommandType::SealRequest;
  } else if (str_type == "plasma_seal_request") {
    return CommandType::PlasmaSealRequest;
  } else if (str_type == "plasma_release_request") {
    return CommandType::PlasmaReleaseRequest;
  } else if (str_type == "plasma_del_data_request") {
    return CommandType::PlasmaDelDataRequest;
  } else if (str_type == "move_buffers_ownership_request") {
    return CommandType::MoveBuffersOwnershipRequest;
  } else if (str_type == "release_request") {
    return CommandType::ReleaseRequest;
  } else if (str_type == "del_data_with_feedbacks_request") {
    return CommandType::DelDataWithFeedbacksRequest;
  } else if (str_type == "is_in_use_request") {
    return CommandType::IsInUseRequest;
  } else if (str_type == "increase_reference_count_request") {
    return CommandType::IncreaseReferenceCountRequest;
  } else if (str_type == "is_spilled_request") {
    return CommandType::IsSpilledRequest;
  } else if (str_type == "create_gpu_buffer_request") {
    return CommandType::CreateGPUBufferRequest;
  } else if (str_type == "get_gpu_buffers_request") {
    return CommandType::GetGPUBuffersRequest;
  } else {
    return CommandType::NullCommand;
  }
}

static inline void encode_msg(const json& root, std::string& msg) {
  msg = json_to_string(root);
}

void WriteErrorReply(Status const& status, std::string& msg) {
  encode_msg(status.ToJSON(), msg);
}

void WriteRegisterRequest(std::string& msg, StoreType const& store_type) {
  json root;
  root["type"] = "register_request";
  root["version"] = vineyard_version();
  root["store_type"] = store_type;

  encode_msg(root, msg);
}

Status ReadRegisterRequest(const json& root, std::string& version,
                           StoreType& store_type) {
  RETURN_ON_ASSERT(root["type"] == "register_request");

  // When the "version" field is missing from the client, we treat it
  // as default unknown version number: 0.0.0.
  version = root.value<std::string>("version", std::string("0.0.0"));

  // Keep backwards compatibility.
  if (root.contains("store_type")) {
    if (root["store_type"].is_number()) {
      store_type = root.value("store_type", /* default */ StoreType::kDefault);
    } else {
      std::string store_type_name =
          root.value("store_type", /* default */ std::string("Normal"));
      if (store_type_name == "Plasma") {
        store_type = StoreType::kPlasma;
      } else {
        store_type = StoreType::kDefault;
      }
    }
  }
  return Status::OK();
}

void WriteRegisterReply(const std::string& ipc_socket,
                        const std::string& rpc_endpoint,
                        const InstanceID instance_id,
                        const SessionID session_id, bool& store_match,
                        std::string& msg) {
  json root;
  root["type"] = "register_reply";
  root["ipc_socket"] = ipc_socket;
  root["rpc_endpoint"] = rpc_endpoint;
  root["instance_id"] = instance_id;
  root["session_id"] = session_id;
  root["version"] = vineyard_version();
  root["store_match"] = store_match;
  encode_msg(root, msg);
}

Status ReadRegisterReply(const json& root, std::string& ipc_socket,
                         std::string& rpc_endpoint, InstanceID& instance_id,
                         SessionID& session_id, std::string& version,
                         bool& store_match) {
  CHECK_IPC_ERROR(root, "register_reply");
  ipc_socket = root["ipc_socket"].get_ref<std::string const&>();
  rpc_endpoint = root["rpc_endpoint"].get_ref<std::string const&>();
  instance_id = root["instance_id"].get<InstanceID>();
  session_id = root["session_id"].get<SessionID>();

  // When the "version" field is missing from the server, we treat it
  // as default unknown version number: 0.0.0.
  version = root.value<std::string>("version", std::string("0.0.0"));
  store_match = root["store_match"].get<bool>();
  return Status::OK();
}

void WriteExitRequest(std::string& msg) {
  json root;
  root["type"] = "exit_request";

  encode_msg(root, msg);
}

void WriteGetDataRequest(const ObjectID id, const bool sync_remote,
                         const bool wait, std::string& msg) {
  json root;
  root["type"] = "get_data_request";
  root["id"] = std::vector<ObjectID>{id};
  root["sync_remote"] = sync_remote;
  root["wait"] = wait;

  encode_msg(root, msg);
}

void WriteGetDataRequest(const std::vector<ObjectID>& ids,
                         const bool sync_remote, const bool wait,
                         std::string& msg) {
  json root;
  root["type"] = "get_data_request";
  root["id"] = ids;
  root["sync_remote"] = sync_remote;
  root["wait"] = wait;

  encode_msg(root, msg);
}

Status ReadGetDataRequest(const json& root, std::vector<ObjectID>& ids,
                          bool& sync_remote, bool& wait) {
  RETURN_ON_ASSERT(root["type"] == "get_data_request");
  ids = root["id"].get_to(ids);
  sync_remote = root.value("sync_remote", false);
  wait = root.value("wait", false);
  return Status::OK();
}

void WriteGetDataReply(const json& content, std::string& msg) {
  json root;
  root["type"] = "get_data_reply";
  root["content"] = content;

  encode_msg(root, msg);
}

Status ReadGetDataReply(const json& root, json& content) {
  CHECK_IPC_ERROR(root, "get_data_reply");
  // should be only one item
  auto content_group = root["content"];
  if (content_group.size() != 1) {
    return Status::ObjectNotExists("failed to read get_data reply: " +
                                   root.dump());
  }
  content = *content_group.begin();
  return Status::OK();
}

Status ReadGetDataReply(const json& root,
                        std::unordered_map<ObjectID, json>& content) {
  CHECK_IPC_ERROR(root, "get_data_reply");
  for (auto const& kv : root["content"].items()) {
    content.emplace(ObjectIDFromString(kv.key()), kv.value());
  }
  return Status::OK();
}

void WriteListDataRequest(std::string const& pattern, bool const regex,
                          size_t const limit, std::string& msg) {
  json root;
  root["type"] = "list_data_request";
  root["pattern"] = pattern;
  root["regex"] = regex;
  root["limit"] = limit;

  encode_msg(root, msg);
}

Status ReadListDataRequest(const json& root, std::string& pattern, bool& regex,
                           size_t& limit) {
  RETURN_ON_ASSERT(root["type"] == "list_data_request");
  pattern = root["pattern"].get_ref<std::string const&>();
  regex = root.value("regex", false);
  limit = root["limit"].get<size_t>();
  return Status::OK();
}

void WriteCreateBufferRequest(const size_t size, std::string& msg) {
  json root;
  root["type"] = "create_buffer_request";
  root["size"] = size;

  encode_msg(root, msg);
}

Status ReadCreateBufferRequest(const json& root, size_t& size) {
  RETURN_ON_ASSERT(root["type"] == "create_buffer_request");
  size = root["size"].get<size_t>();
  return Status::OK();
}

void WriteCreateBufferReply(const ObjectID id,
                            const std::shared_ptr<Payload>& object,
                            const int fd_to_send, std::string& msg) {
  json root;
  root["type"] = "create_buffer_reply";
  root["id"] = id;
  root["fd"] = fd_to_send;
  json tree;
  object->ToJSON(tree);
  root["created"] = tree;

  encode_msg(root, msg);
}

Status ReadCreateBufferReply(const json& root, ObjectID& id, Payload& object,
                             int& fd_sent) {
  CHECK_IPC_ERROR(root, "create_buffer_reply");
  json tree = root["created"];
  id = root["id"].get<ObjectID>();
  object.FromJSON(tree);
  fd_sent = root.value("fd", -1);
  return Status::OK();
}

void WriteCreateDiskBufferRequest(const size_t size, const std::string& path,
                                  std::string& msg) {
  json root;
  root["type"] = "create_disk_buffer_request";
  root["size"] = size;
  root["path"] = path;

  encode_msg(root, msg);
}

Status ReadCreateDiskBufferRequest(const json& root, size_t& size,
                                   std::string& path) {
  RETURN_ON_ASSERT(root["type"] == "create_disk_buffer_request");
  size = root["size"].get<size_t>();
  path = root["path"].get<std::string>();
  return Status::OK();
}

void WriteCreateDiskBufferReply(const ObjectID id,
                                const std::shared_ptr<Payload>& object,
                                const int fd_to_send, std::string& msg) {
  json root;
  root["type"] = "create_disk_buffer_reply";
  root["id"] = id;
  root["fd"] = fd_to_send;
  json tree;
  object->ToJSON(tree);
  root["created"] = tree;

  encode_msg(root, msg);
}

Status ReadCreateDiskBufferReply(const json& root, ObjectID& id,
                                 Payload& object, int& fd_sent) {
  CHECK_IPC_ERROR(root, "create_disk_buffer_reply");
  json tree = root["created"];
  id = root["id"].get<ObjectID>();
  object.FromJSON(tree);
  fd_sent = root.value("fd", -1);
  return Status::OK();
}

// GPU related implementations
void WriteCreateGPUBufferRequest(const size_t size, std::string& msg) {
  json root;
  root["type"] = "create_gpu_buffer_request";
  root["size"] = size;

  encode_msg(root, msg);
}

Status ReadCreateGPUBufferRequest(const json& root, size_t& size) {
  RETURN_ON_ASSERT(root["type"] == "create_gpu_buffer_request");
  size = root["size"].get<size_t>();
  return Status::OK();
}

void WriteGPUCreateBufferReply(const ObjectID id,
                               const std::shared_ptr<Payload>& object,
                               GPUUnifiedAddress uva, std::string& msg) {
  json root;
  root["type"] = "create_gpu_buffer_reply";
  root["id"] = id;
  std::cout << std::endl;
  root["handle"] = uva.getIpcHandleVec();
  json tree;
  object->ToJSON(tree);
  root["created"] = tree;
  encode_msg(root, msg);
}

Status ReadGPUCreateBufferReply(
    const json& root, ObjectID& id, Payload& Object,
    std::shared_ptr<vineyard::GPUUnifiedAddress> uva) {
  json tree = root["created"];
  id = root["id"].get<ObjectID>();
  Object.FromJSON(tree);
  std::vector<int64_t> handle_vec = root["handle"].get<std::vector<int64_t>>();
  uva->setIpcHandleVec(handle_vec);
  uva->setSize(Object.data_size);
  return Status::OK();
}

void WriteGetGPUBuffersRequest(const std::set<ObjectID>& ids, const bool unsafe,
                               std::string& msg) {
  json root;
  root["type"] = "get_gpu_buffers_request";
  int idx = 0;
  for (auto const& id : ids) {
    root[std::to_string(idx++)] = id;
  }
  root["num"] = ids.size();
  root["unsafe"] = unsafe;

  encode_msg(root, msg);
}

Status ReadGetGPUBuffersRequest(const json& root, std::vector<ObjectID>& ids,
                                bool& unsafe) {
  RETURN_ON_ASSERT(root["type"] == "get_gpu_buffers_request");
  size_t num = root["num"].get<size_t>();
  for (size_t i = 0; i < num; ++i) {
    ids.push_back(root[std::to_string(i)].get<ObjectID>());
  }
  unsafe = root.value("unsafe", false);
  return Status::OK();
}

void WriteGetGPUBuffersReply(
    const std::vector<std::shared_ptr<Payload>>& objects,
    const std::vector<std::vector<int64_t>>& handle_to_send, std::string& msg) {
  json root;
  root["type"] = "get_gpu_buffers_reply";
  for (size_t i = 0; i < objects.size(); ++i) {
    json tree;
    objects[i]->ToJSON(tree);
    root[std::to_string(i)] = tree;
  }
  root["handles"] = handle_to_send;
  root["num"] = objects.size();

  encode_msg(root, msg);
}

Status ReadGetGPUBuffersReply(const json& root, std::vector<Payload>& objects,
                              std::vector<GPUUnifiedAddress>& gua_sent) {
  RETURN_ON_ASSERT(root["type"] == "get_gpu_buffers_reply");
  for (size_t i = 0; i < root["num"]; ++i) {
    json tree = root[std::to_string(i)];
    Payload object;
    object.FromJSON(tree);
    objects.emplace_back(object);
  }
  std::vector<std::vector<int64_t>> handle_vec;
  if (root.contains("handles")) {
    handle_vec = root["handles"].get<std::vector<std::vector<int64_t>>>();
  }
  // get cuda handle
  for (size_t i = 0; i < root["num"]; i++) {
    GPUUnifiedAddress gua(false);
    gua.setIpcHandleVec(handle_vec[i]);
    gua.setSize(objects[i].data_size);
    gua_sent.emplace_back(gua);
  }
  return Status::OK();
}

void WriteCreateRemoteBufferRequest(const size_t size, std::string& msg) {
  json root;
  root["type"] = "create_remote_buffer_request";
  root["size"] = size;

  encode_msg(root, msg);
}

Status ReadCreateRemoteBufferRequest(const json& root, size_t& size) {
  RETURN_ON_ASSERT(root["type"] == "create_remote_buffer_request");
  size = root["size"].get<size_t>();
  return Status::OK();
}

void WriteGetBuffersRequest(const std::set<ObjectID>& ids, const bool unsafe,
                            std::string& msg) {
  json root;
  root["type"] = "get_buffers_request";
  int idx = 0;
  for (auto const& id : ids) {
    root[std::to_string(idx++)] = id;
  }
  root["num"] = ids.size();
  root["unsafe"] = unsafe;

  encode_msg(root, msg);
}

void WriteGetBuffersRequest(const std::unordered_set<ObjectID>& ids,
                            const bool unsafe, std::string& msg) {
  json root;
  root["type"] = "get_buffers_request";
  int idx = 0;
  for (auto const& id : ids) {
    root[std::to_string(idx++)] = id;
  }
  root["num"] = ids.size();
  root["unsafe"] = unsafe;

  encode_msg(root, msg);
}

Status ReadGetBuffersRequest(const json& root, std::vector<ObjectID>& ids,
                             bool& unsafe) {
  RETURN_ON_ASSERT(root["type"] == "get_buffers_request");
  size_t num = root["num"].get<size_t>();
  for (size_t i = 0; i < num; ++i) {
    ids.push_back(root[std::to_string(i)].get<ObjectID>());
  }
  unsafe = root.value("unsafe", false);
  return Status::OK();
}

void WriteGetBuffersReply(const std::vector<std::shared_ptr<Payload>>& objects,
                          const std::vector<int>& fd_to_send,
                          std::string& msg) {
  json root;
  root["type"] = "get_buffers_reply";
  for (size_t i = 0; i < objects.size(); ++i) {
    json tree;
    objects[i]->ToJSON(tree);
    root[std::to_string(i)] = tree;
  }
  root["fds"] = fd_to_send;
  root["num"] = objects.size();

  encode_msg(root, msg);
}

Status ReadGetBuffersReply(const json& root, std::vector<Payload>& objects,
                           std::vector<int>& fd_sent) {
  CHECK_IPC_ERROR(root, "get_buffers_reply");

  for (size_t i = 0; i < root.value("num", static_cast<size_t>(0)); ++i) {
    json tree = root[std::to_string(i)];
    Payload object;
    object.FromJSON(tree);
    objects.emplace_back(object);
  }
  if (root.contains("fds")) {
    fd_sent = root["fds"].get<std::vector<int>>();
  }
  return Status::OK();
}

void WriteGetRemoteBuffersRequest(const std::set<ObjectID>& ids,
                                  const bool unsafe, std::string& msg) {
  json root;
  root["type"] = "get_remote_buffers_request";
  int idx = 0;
  for (auto const& id : ids) {
    root[std::to_string(idx++)] = id;
  }
  root["num"] = ids.size();
  root["unsafe"] = unsafe;

  encode_msg(root, msg);
}

void WriteGetRemoteBuffersRequest(const std::unordered_set<ObjectID>& ids,
                                  const bool unsafe, std::string& msg) {
  json root;
  root["type"] = "get_remote_buffers_request";
  int idx = 0;
  for (auto const& id : ids) {
    root[std::to_string(idx++)] = id;
  }
  root["num"] = ids.size();
  root["unsafe"] = unsafe;

  encode_msg(root, msg);
}

Status ReadGetRemoteBuffersRequest(const json& root, std::vector<ObjectID>& ids,
                                   bool& unsafe) {
  RETURN_ON_ASSERT(root["type"] == "get_remote_buffers_request");
  size_t num = root["num"].get<size_t>();
  for (size_t i = 0; i < num; ++i) {
    ids.push_back(root[std::to_string(i)].get<ObjectID>());
  }
  unsafe = root.value("unsafe", false);
  return Status::OK();
}

void WriteDropBufferRequest(const ObjectID id, std::string& msg) {
  json root;
  root["type"] = "drop_buffer_request";
  root["id"] = id;

  encode_msg(root, msg);
}

Status ReadDropBufferRequest(const json& root, ObjectID& id) {
  RETURN_ON_ASSERT(root["type"] == "drop_buffer_request");
  id = root["id"].get<ObjectID>();
  return Status::OK();
}

void WriteDropBufferReply(std::string& msg) {
  json root;
  root["type"] = "drop_buffer_reply";

  encode_msg(root, msg);
}

Status ReadDropBufferReply(const json& root) {
  CHECK_IPC_ERROR(root, "drop_buffer_reply");
  return Status::OK();
}

void WriteCreateDataRequest(const json& content, std::string& msg) {
  json root;
  root["type"] = "create_data_request";
  root["content"] = content;

  encode_msg(root, msg);
}

Status ReadCreateDataRequest(const json& root, json& content) {
  RETURN_ON_ASSERT(root["type"] == "create_data_request");
  content = root["content"];
  return Status::OK();
}

void WriteCreateDataReply(const ObjectID& id, const Signature& signature,
                          const InstanceID& instance_id, std::string& msg) {
  json root;
  root["type"] = "create_data_reply";
  root["id"] = id;
  root["signature"] = signature;
  root["instance_id"] = instance_id;

  encode_msg(root, msg);
}

Status ReadCreateDataReply(const json& root, ObjectID& id, Signature& signature,
                           InstanceID& instance_id) {
  CHECK_IPC_ERROR(root, "create_data_reply");
  id = root["id"].get<ObjectID>();
  signature = root["signature"].get<Signature>();
  instance_id = root["instance_id"].get<InstanceID>();
  return Status::OK();
}

void WritePersistRequest(const ObjectID id, std::string& msg) {
  json root;
  root["type"] = "persist_request";
  root["id"] = id;

  encode_msg(root, msg);
}

Status ReadPersistRequest(const json& root, ObjectID& id) {
  RETURN_ON_ASSERT(root["type"] == "persist_request");
  id = root["id"].get<ObjectID>();
  return Status::OK();
}

void WritePersistReply(std::string& msg) {
  json root;
  root["type"] = "persist_reply";

  encode_msg(root, msg);
}

Status ReadPersistReply(const json& root) {
  CHECK_IPC_ERROR(root, "persist_reply");
  return Status::OK();
}

void WriteIfPersistRequest(const ObjectID id, std::string& msg) {
  json root;
  root["type"] = "if_persist_request";
  root["id"] = id;

  encode_msg(root, msg);
}

Status ReadIfPersistRequest(const json& root, ObjectID& id) {
  RETURN_ON_ASSERT(root["type"] == "if_persist_request");
  id = root["id"].get<ObjectID>();
  return Status::OK();
}

void WriteIfPersistReply(bool persist, std::string& msg) {
  json root;
  root["type"] = "if_persist_reply";
  root["persist"] = persist;

  encode_msg(root, msg);
}

Status ReadIfPersistReply(const json& root, bool& persist) {
  CHECK_IPC_ERROR(root, "if_persist_reply");
  persist = root.value("persist", false);
  return Status::OK();
}

void WriteExistsRequest(const ObjectID id, std::string& msg) {
  json root;
  root["type"] = "exists_request";
  root["id"] = id;

  encode_msg(root, msg);
}

Status ReadExistsRequest(const json& root, ObjectID& id) {
  RETURN_ON_ASSERT(root["type"] == "exists_request");
  id = root["id"].get<ObjectID>();
  return Status::OK();
}

void WriteExistsReply(bool exists, std::string& msg) {
  json root;
  root["type"] = "exists_reply";
  root["exists"] = exists;

  encode_msg(root, msg);
}

Status ReadExistsReply(const json& root, bool& exists) {
  CHECK_IPC_ERROR(root, "exists_reply");
  exists = root.value("exists", false);
  return Status::OK();
}

void WriteDelDataRequest(const ObjectID id, const bool force, const bool deep,
                         const bool fastpath, std::string& msg) {
  json root;
  root["type"] = "del_data_request";
  root["id"] = std::vector<ObjectID>{id};
  root["force"] = force;
  root["deep"] = deep;
  root["fastpath"] = fastpath;

  encode_msg(root, msg);
}

void WriteDelDataRequest(const std::vector<ObjectID>& ids, const bool force,
                         const bool deep, const bool fastpath,
                         std::string& msg) {
  json root;
  root["type"] = "del_data_request";
  root["id"] = ids;
  root["force"] = force;
  root["deep"] = deep;
  root["fastpath"] = fastpath;

  encode_msg(root, msg);
}

Status ReadDelDataRequest(const json& root, std::vector<ObjectID>& ids,
                          bool& force, bool& deep, bool& fastpath) {
  RETURN_ON_ASSERT(root["type"] == "del_data_request");
  ids = root["id"].get_to(ids);
  force = root.value("force", false);
  deep = root.value("deep", false);
  fastpath = root.value("fastpath", false);
  return Status::OK();
}

void WriteDelDataReply(std::string& msg) {
  json root;
  root["type"] = "del_data_reply";

  encode_msg(root, msg);
}

Status ReadDelDataReply(const json& root) {
  CHECK_IPC_ERROR(root, "del_data_reply");
  return Status::OK();
}

void WriteClusterMetaRequest(std::string& msg) {
  json root;
  root["type"] = "cluster_meta";

  encode_msg(root, msg);
}

Status ReadClusterMetaRequest(const json& root) {
  RETURN_ON_ASSERT(root["type"] == "cluster_meta");
  return Status::OK();
}

void WriteClusterMetaReply(const json& meta, std::string& msg) {
  json root;
  root["type"] = "cluster_meta";
  root["meta"] = meta;

  encode_msg(root, msg);
}

Status ReadClusterMetaReply(const json& root, json& meta) {
  CHECK_IPC_ERROR(root, "cluster_meta");
  meta = root["meta"];
  return Status::OK();
}

void WriteInstanceStatusRequest(std::string& msg) {
  json root;
  root["type"] = "instance_status_request";

  encode_msg(root, msg);
}

Status ReadInstanceStatusRequest(const json& root) {
  RETURN_ON_ASSERT(root["type"] == "instance_status_request");
  return Status::OK();
}

void WriteInstanceStatusReply(const json& meta, std::string& msg) {
  json root;
  root["type"] = "instance_status_reply";
  root["meta"] = meta;

  encode_msg(root, msg);
}

Status ReadInstanceStatusReply(const json& root, json& meta) {
  CHECK_IPC_ERROR(root, "instance_status_reply");
  meta = root["meta"];
  return Status::OK();
}

void WritePutNameRequest(const ObjectID object_id, const std::string& name,
                         std::string& msg) {
  json root;
  root["type"] = "put_name_request";
  root["object_id"] = object_id;
  root["name"] = name;

  encode_msg(root, msg);
}

Status ReadPutNameRequest(const json& root, ObjectID& object_id,
                          std::string& name) {
  RETURN_ON_ASSERT(root["type"] == "put_name_request");
  object_id = root["object_id"].get<ObjectID>();
  name = root["name"].get_ref<std::string const&>();
  return Status::OK();
}

void WritePutNameReply(std::string& msg) {
  json root;
  root["type"] = "put_name_reply";

  encode_msg(root, msg);
}

Status ReadPutNameReply(const json& root) {
  CHECK_IPC_ERROR(root, "put_name_reply");
  return Status::OK();
}

void WriteGetNameRequest(const std::string& name, const bool wait,
                         std::string& msg) {
  json root;
  root["type"] = "get_name_request";
  root["name"] = name;
  root["wait"] = wait;

  encode_msg(root, msg);
}

Status ReadGetNameRequest(const json& root, std::string& name, bool& wait) {
  RETURN_ON_ASSERT(root["type"] == "get_name_request");
  name = root["name"].get_ref<std::string const&>();
  wait = root["wait"].get<bool>();
  return Status::OK();
}

void WriteGetNameReply(const ObjectID& object_id, std::string& msg) {
  json root;
  root["type"] = "get_name_reply";
  root["object_id"] = object_id;

  encode_msg(root, msg);
}

Status ReadGetNameReply(const json& root, ObjectID& object_id) {
  CHECK_IPC_ERROR(root, "get_name_reply");
  object_id = root["object_id"].get<ObjectID>();
  return Status::OK();
}

void WriteDropNameRequest(const std::string& name, std::string& msg) {
  json root;
  root["type"] = "drop_name_request";
  root["name"] = name;

  encode_msg(root, msg);
}

Status ReadDropNameRequest(const json& root, std::string& name) {
  RETURN_ON_ASSERT(root["type"] == "drop_name_request");
  name = root["name"].get_ref<std::string const&>();
  return Status::OK();
}

void WriteDropNameReply(std::string& msg) {
  json root;
  root["type"] = "drop_name_reply";

  encode_msg(root, msg);
}

Status ReadDropNameReply(const json& root) {
  CHECK_IPC_ERROR(root, "drop_name_reply");
  return Status::OK();
}

void WriteMigrateObjectRequest(const ObjectID object_id, const bool local,
                               const bool is_stream, const std::string& peer,
                               std::string const& peer_rpc_endpoint,
                               std::string& msg) {
  json root;
  root["type"] = "migrate_object_request";
  root["object_id"] = object_id;
  root["local"] = local;
  root["is_stream"] = is_stream;
  root["peer"] = peer;
  root["peer_rpc_endpoint"] = peer_rpc_endpoint,

  encode_msg(root, msg);
}

Status ReadMigrateObjectRequest(const json& root, ObjectID& object_id,
                                bool& local, bool& is_stream, std::string& peer,
                                std::string& peer_rpc_endpoint) {
  RETURN_ON_ASSERT(root["type"].get_ref<std::string const&>() ==
                   "migrate_object_request");
  object_id = root["object_id"].get<ObjectID>();
  local = root["local"].get<bool>();
  is_stream = root["is_stream"].get<bool>();
  peer = root["peer"].get_ref<std::string const&>();
  peer_rpc_endpoint = root["peer_rpc_endpoint"].get_ref<std::string const&>();
  return Status::OK();
}

void WriteMigrateObjectReply(const ObjectID& object_id, std::string& msg) {
  json root;
  root["type"] = "migrate_object_reply";
  root["object_id"] = object_id;

  encode_msg(root, msg);
}

Status ReadMigrateObjectReply(const json& root, ObjectID& object_id) {
  CHECK_IPC_ERROR(root, "migrate_object_reply");
  object_id = root["object_id"].get<ObjectID>();
  return Status::OK();
}

void WriteCreateStreamRequest(const ObjectID& object_id, std::string& msg) {
  json root;
  root["type"] = "create_stream_request";
  root["object_id"] = object_id;

  encode_msg(root, msg);
}

Status ReadCreateStreamRequest(const json& root, ObjectID& object_id) {
  RETURN_ON_ASSERT(root["type"] == "create_stream_request");
  object_id = root["object_id"].get<ObjectID>();
  return Status::OK();
}

void WriteCreateStreamReply(std::string& msg) {
  json root;
  root["type"] = "create_stream_reply";

  encode_msg(root, msg);
}

Status ReadCreateStreamReply(const json& root) {
  CHECK_IPC_ERROR(root, "create_stream_reply");
  return Status::OK();
}

void WriteOpenStreamRequest(const ObjectID& object_id, const int64_t& mode,
                            std::string& msg) {
  json root;
  root["type"] = "open_stream_request";
  root["object_id"] = object_id;
  root["mode"] = mode;

  encode_msg(root, msg);
}

Status ReadOpenStreamRequest(const json& root, ObjectID& object_id,
                             int64_t& mode) {
  RETURN_ON_ASSERT(root["type"] == "open_stream_request");
  object_id = root["object_id"].get<ObjectID>();
  mode = root["mode"].get<int64_t>();
  return Status::OK();
}

void WriteOpenStreamReply(std::string& msg) {
  json root;
  root["type"] = "open_stream_reply";

  encode_msg(root, msg);
}

Status ReadOpenStreamReply(const json& root) {
  CHECK_IPC_ERROR(root, "open_stream_reply");
  return Status::OK();
}

void WriteGetNextStreamChunkRequest(const ObjectID stream_id, const size_t size,
                                    std::string& msg) {
  json root;
  root["type"] = "get_next_stream_chunk_request";
  root["id"] = stream_id;
  root["size"] = size;

  encode_msg(root, msg);
}

Status ReadGetNextStreamChunkRequest(const json& root, ObjectID& stream_id,
                                     size_t& size) {
  RETURN_ON_ASSERT(root["type"] == "get_next_stream_chunk_request");
  stream_id = root["id"].get<ObjectID>();
  size = root["size"].get<size_t>();
  return Status::OK();
}

void WriteGetNextStreamChunkReply(std::shared_ptr<Payload> const& object,
                                  int fd_sent, std::string& msg) {
  json root;
  root["type"] = "get_next_stream_chunk_reply";
  json buffer_meta;
  object->ToJSON(buffer_meta);
  root["buffer"] = buffer_meta;
  root["fd"] = fd_sent;

  encode_msg(root, msg);
}

Status ReadGetNextStreamChunkReply(const json& root, Payload& object,
                                   int& fd_sent) {
  CHECK_IPC_ERROR(root, "get_next_stream_chunk_reply");
  object.FromJSON(root["buffer"]);
  fd_sent = root.value("fd", -1);
  return Status::OK();
}

void WritePushNextStreamChunkRequest(const ObjectID stream_id,
                                     const ObjectID chunk, std::string& msg) {
  json root;
  root["type"] = "push_next_stream_chunk_request";
  root["id"] = stream_id;
  root["chunk"] = chunk;

  encode_msg(root, msg);
}

Status ReadPushNextStreamChunkRequest(const json& root, ObjectID& stream_id,
                                      ObjectID& chunk) {
  RETURN_ON_ASSERT(root["type"] == "push_next_stream_chunk_request");
  stream_id = root["id"].get<ObjectID>();
  chunk = root["chunk"].get<ObjectID>();
  return Status::OK();
}

void WritePushNextStreamChunkReply(std::string& msg) {
  json root;
  root["type"] = "push_next_stream_chunk_reply";
  encode_msg(root, msg);
}

Status ReadPushNextStreamChunkReply(const json& root) {
  CHECK_IPC_ERROR(root, "push_next_stream_chunk_reply");
  return Status::OK();
}

void WritePullNextStreamChunkRequest(const ObjectID stream_id,
                                     std::string& msg) {
  json root;
  root["type"] = "pull_next_stream_chunk_request";
  root["id"] = stream_id;

  encode_msg(root, msg);
}

Status ReadPullNextStreamChunkRequest(const json& root, ObjectID& stream_id) {
  RETURN_ON_ASSERT(root["type"] == "pull_next_stream_chunk_request");
  stream_id = root["id"].get<ObjectID>();
  return Status::OK();
}

void WritePullNextStreamChunkReply(ObjectID const chunk, std::string& msg) {
  json root;
  root["type"] = "pull_next_stream_chunk_reply";
  root["chunk"] = chunk;

  encode_msg(root, msg);
}

Status ReadPullNextStreamChunkReply(const json& root, ObjectID& chunk) {
  CHECK_IPC_ERROR(root, "pull_next_stream_chunk_reply");
  chunk = root["chunk"].get<ObjectID>();
  return Status::OK();
}

void WriteStopStreamRequest(const ObjectID stream_id, const bool failed,
                            std::string& msg) {
  json root;
  root["type"] = "stop_stream_request";
  root["id"] = stream_id;
  root["failed"] = failed;

  encode_msg(root, msg);
}

Status ReadStopStreamRequest(const json& root, ObjectID& stream_id,
                             bool& failed) {
  RETURN_ON_ASSERT(root["type"] == "stop_stream_request");
  stream_id = root["id"].get<ObjectID>();
  failed = root["failed"].get<bool>();
  return Status::OK();
}

void WriteStopStreamReply(std::string& msg) {
  json root;
  root["type"] = "stop_stream_reply";

  encode_msg(root, msg);
}

Status ReadStopStreamReply(const json& root) {
  CHECK_IPC_ERROR(root, "stop_stream_reply");
  return Status::OK();
}

void WriteShallowCopyRequest(const ObjectID id, std::string& msg) {
  json root;
  root["type"] = "shallow_copy_request";
  root["id"] = id;

  encode_msg(root, msg);
}

void WriteShallowCopyRequest(const ObjectID id, json const& extra_metadata,
                             std::string& msg) {
  json root;
  root["type"] = "shallow_copy_request";
  root["id"] = id;
  root["extra"] = extra_metadata;

  encode_msg(root, msg);
}

Status ReadShallowCopyRequest(const json& root, ObjectID& id,
                              json& extra_metadata) {
  RETURN_ON_ASSERT(root["type"] == "shallow_copy_request");
  id = root["id"].get<ObjectID>();
  extra_metadata = root.value("extra", json::object());
  return Status::OK();
}

void WriteShallowCopyReply(const ObjectID target_id, std::string& msg) {
  json root;
  root["type"] = "shallow_copy_reply";
  root["target_id"] = target_id;

  encode_msg(root, msg);
}

Status ReadShallowCopyReply(const json& root, ObjectID& target_id) {
  CHECK_IPC_ERROR(root, "shallow_copy_reply");
  target_id = root["target_id"].get<ObjectID>();
  return Status::OK();
}

void WriteMakeArenaRequest(const size_t size, std::string& msg) {
  json root;
  root["type"] = "make_arena_request";
  root["size"] = size;

  encode_msg(root, msg);
}

Status ReadMakeArenaRequest(const json& root, size_t& size) {
  RETURN_ON_ASSERT(root["type"] == "make_arena_request");
  size = root["size"].get<size_t>();
  return Status::OK();
}

void WriteMakeArenaReply(const int fd, const size_t size, const uintptr_t base,
                         std::string& msg) {
  json root;
  root["type"] = "make_arena_reply";
  root["fd"] = fd;
  root["size"] = size;
  root["base"] = base;

  encode_msg(root, msg);
}

Status ReadMakeArenaReply(const json& root, int& fd, size_t& size,
                          uintptr_t& base) {
  CHECK_IPC_ERROR(root, "make_arena_reply");
  fd = root["fd"].get<int>();
  size = root["size"].get<size_t>();
  base = root["base"].get<uintptr_t>();
  return Status::OK();
}

void WriteFinalizeArenaRequest(const int fd, std::vector<size_t> const& offsets,
                               std::vector<size_t> const& sizes,
                               std::string& msg) {
  json root;
  root["type"] = "finalize_arena_request";
  root["fd"] = fd;
  root["offsets"] = offsets;
  root["sizes"] = sizes;

  encode_msg(root, msg);
}

Status ReadFinalizeArenaRequest(const json& root, int& fd,
                                std::vector<size_t>& offsets,
                                std::vector<size_t>& sizes) {
  RETURN_ON_ASSERT(root["type"] == "finalize_arena_request");
  fd = root["fd"].get<int>();
  offsets = root["offsets"].get<std::vector<size_t>>();
  sizes = root["sizes"].get<std::vector<size_t>>();
  return Status::OK();
}

void WriteFinalizeArenaReply(std::string& msg) {
  json root;
  root["type"] = "finalize_arena_reply";
  encode_msg(root, msg);
}

Status ReadFinalizeArenaReply(const json& root) {
  CHECK_IPC_ERROR(root, "finalize_arena_reply");
  return Status::OK();
}

void WriteClearRequest(std::string& msg) {
  json root;
  root["type"] = "clear_request";

  encode_msg(root, msg);
}

Status ReadClearRequest(const json& root) {
  RETURN_ON_ASSERT(root["type"] == "clear_request");
  return Status::OK();
}

void WriteClearReply(std::string& msg) {
  json root;
  root["type"] = "clear_reply";
  encode_msg(root, msg);
}

Status ReadClearReply(const json& root) {
  CHECK_IPC_ERROR(root, "clear_reply");
  return Status::OK();
}

void WriteDebugRequest(const json& debug, std::string& msg) {
  json root;
  root["type"] = "debug_command";
  root["debug"] = debug;
  encode_msg(root, msg);
}

Status ReadDebugRequest(const json& root, json& debug) {
  RETURN_ON_ASSERT(root["type"] == "debug_command");
  debug = root["debug"];
  return Status::OK();
}

void WriteDebugReply(const json& result, std::string& msg) {
  json root;
  root["type"] = "debug_reply";
  root["result"] = result;
  encode_msg(root, msg);
}

Status ReadDebugReply(const json& root, json& result) {
  CHECK_IPC_ERROR(root, "debug_reply");
  result = root["result"];
  return Status::OK();
}

void WriteNewSessionRequest(std::string& msg,
                            StoreType const& bulk_store_type) {
  json root;
  root["type"] = "new_session_request";
  root["bulk_store_type"] = bulk_store_type;
  encode_msg(root, msg);
}

Status ReadNewSessionRequest(json const& root, StoreType& bulk_store_type) {
  RETURN_ON_ASSERT(root["type"] == "new_session_request");
  bulk_store_type =
      root.value("bulk_store_type", /* default */ StoreType::kDefault);
  return Status::OK();
}

void WriteNewSessionReply(std::string& msg, std::string const& socket_path) {
  json root;
  root["type"] = "new_session_reply";
  root["socket_path"] = socket_path;
  encode_msg(root, msg);
}

Status ReadNewSessionReply(const json& root, std::string& socket_path) {
  CHECK_IPC_ERROR(root, "new_session_reply");
  socket_path = root["socket_path"].get_ref<std::string const&>();
  return Status::OK();
}

void WriteDeleteSessionRequest(std::string& msg) {
  json root;
  root["type"] = "delete_session_request";
  encode_msg(root, msg);
}

void WriteDeleteSessionReply(std::string& msg) {
  json root;
  root["type"] = "delete_session_reply";
  encode_msg(root, msg);
}

void WriteCreateBufferByPlasmaRequest(PlasmaID const plasma_id,
                                      size_t const size,
                                      size_t const plasma_size,
                                      std::string& msg) {
  json root;
  root["type"] = "create_buffer_by_plasma_request";
  root["plasma_id"] = plasma_id;
  root["plasma_size"] = plasma_size;
  root["size"] = size;

  encode_msg(root, msg);
}

Status ReadCreateBufferByPlasmaRequest(json const& root, PlasmaID& plasma_id,
                                       size_t& size, size_t& plasma_size) {
  RETURN_ON_ASSERT(root["type"] == "create_buffer_by_plasma_request");
  plasma_id = root["plasma_id"].get<PlasmaID>();
  size = root["size"].get<size_t>();
  plasma_size = root["plasma_size"].get<size_t>();

  return Status::OK();
}

void WriteCreateBufferByPlasmaReply(
    ObjectID const object_id,
    const std::shared_ptr<PlasmaPayload>& plasma_object, int fd_to_send,
    std::string& msg) {
  json root;
  root["type"] = "create_buffer_by_plasma_reply";
  root["id"] = object_id;
  json tree;
  plasma_object->ToJSON(tree);
  root["created"] = tree;
  root["fd"] = fd_to_send;

  encode_msg(root, msg);
}

Status ReadCreateBufferByPlasmaReply(json const& root, ObjectID& object_id,
                                     PlasmaPayload& plasma_object,
                                     int& fd_sent) {
  CHECK_IPC_ERROR(root, "create_buffer_by_plasma_reply");
  json tree = root["created"];
  object_id = root["id"].get<ObjectID>();
  plasma_object.FromJSON(tree);
  fd_sent = root.value("fd", -1);
  return Status::OK();
}

void WriteGetBuffersByPlasmaRequest(std::set<PlasmaID> const& plasma_ids,
                                    const bool unsafe, std::string& msg) {
  json root;
  root["type"] = "get_buffers_by_plasma_request";
  int idx = 0;
  for (auto const& eid : plasma_ids) {
    root[std::to_string(idx++)] = eid;
  }
  root["num"] = plasma_ids.size();
  root["unsafe"] = unsafe;

  encode_msg(root, msg);
}

Status ReadGetBuffersByPlasmaRequest(const json& root,
                                     std::vector<PlasmaID>& plasma_ids,
                                     bool& unsafe) {
  RETURN_ON_ASSERT(root["type"] == "get_buffers_by_plasma_request");
  size_t num = root["num"].get<size_t>();
  for (size_t i = 0; i < num; ++i) {
    plasma_ids.push_back(root[std::to_string(i)].get<PlasmaID>());
  }
  unsafe = root.value("unsafe", false);
  return Status::OK();
}

void WriteGetBuffersByPlasmaReply(
    std::vector<std::shared_ptr<PlasmaPayload>> const& plasma_objects,
    std::string& msg) {
  json root;
  root["type"] = "get_buffers_by_plasma_reply";
  for (size_t i = 0; i < plasma_objects.size(); ++i) {
    json tree;
    plasma_objects[i]->ToJSON(tree);
    root[std::to_string(i)] = tree;
  }
  root["num"] = plasma_objects.size();

  encode_msg(root, msg);
}

Status ReadGetBuffersByPlasmaReply(json const& root,
                                   std::vector<PlasmaPayload>& plasma_objects) {
  CHECK_IPC_ERROR(root, "get_buffers_by_plasma_reply");
  for (size_t i = 0; i < root["num"]; ++i) {
    json tree = root[std::to_string(i)];
    PlasmaPayload plasma_object;
    plasma_object.FromJSON(tree);
    plasma_objects.emplace_back(plasma_object);
  }
  return Status::OK();
}

void WriteSealRequest(ObjectID const& object_id, std::string& msg) {
  json root;
  root["type"] = "seal_request";
  root["object_id"] = object_id;
  encode_msg(root, msg);
}

Status ReadSealRequest(json const& root, ObjectID& object_id) {
  RETURN_ON_ASSERT(root["type"] == "seal_request");
  object_id = root["object_id"].get<ObjectID>();
  return Status::OK();
}

void WritePlasmaSealRequest(PlasmaID const& plasma_id, std::string& msg) {
  json root;
  root["type"] = "plasma_seal_request";
  root["plasma_id"] = plasma_id;
  encode_msg(root, msg);
}

Status ReadPlasmaSealRequest(json const& root, PlasmaID& plasma_id) {
  RETURN_ON_ASSERT(root["type"] == "plasma_seal_request");
  plasma_id = root["plasma_id"].get<PlasmaID>();
  return Status::OK();
}

void WriteSealReply(std::string& msg) {
  json root;
  root["type"] = "seal_reply";
  encode_msg(root, msg);
}

Status ReadSealReply(json const& root) {
  RETURN_ON_ASSERT(root["type"] == "seal_reply");
  return Status::OK();
}

void WritePlasmaReleaseRequest(PlasmaID const& plasma_id, std::string& msg) {
  json root;
  root["type"] = "plasma_release_request";
  root["plasma_id"] = plasma_id;
  encode_msg(root, msg);
}

Status ReadPlasmaReleaseRequest(json const& root, PlasmaID& plasma_id) {
  RETURN_ON_ASSERT(root["type"] == "plasma_release_request");
  plasma_id = root["plasma_id"].get<PlasmaID>();
  return Status::OK();
}

void WritePlasmaReleaseReply(std::string& msg) {
  json root;
  root["type"] = "plasma_release_reply";
  encode_msg(root, msg);
}

Status ReadPlasmaReleaseReply(json const& root) {
  CHECK_IPC_ERROR(root, "plasma_release_reply");
  return Status::OK();
}

void WritePlasmaDelDataRequest(PlasmaID const& plasma_id, std::string& msg) {
  json root;
  root["type"] = "plasma_delete_data_request";
  root["plasma_id"] = plasma_id;
  encode_msg(root, msg);
}

Status ReadPlasmaDelDataRequest(json const& root, PlasmaID& plasma_id) {
  RETURN_ON_ASSERT(root["type"] == "plasma_delete_data_request");
  plasma_id = root["plasma_id"].get<PlasmaID>();
  return Status::OK();
}

void WritePlasmaDelDataReply(std::string& msg) {
  json root;
  root["type"] = "plasma_delete_data_reply";
  encode_msg(root, msg);
}

Status ReadPlasmaDelDataReply(json const& root) {
  CHECK_IPC_ERROR(root, "plasma_delete_data_reply");
  return Status::OK();
}

void WriteMoveBuffersOwnershipRequest(
    std::map<ObjectID, ObjectID> const& id_to_id, SessionID const session_id,
    std::string& msg) {
  json root;
  root["type"] = "move_buffers_ownership_request";
  root["id_to_id"] = id_to_id;
  root["session_id"] = session_id;
  encode_msg(root, msg);
}

void WriteMoveBuffersOwnershipRequest(
    std::map<ObjectID, PlasmaID> const& id_to_pid, SessionID const session_id,
    std::string& msg) {
  json root;
  root["type"] = "move_buffers_ownership_request";
  root["id_to_pid"] = id_to_pid;
  root["session_id"] = session_id;
  encode_msg(root, msg);
}

void WriteMoveBuffersOwnershipRequest(
    std::map<PlasmaID, ObjectID> const& pid_to_id, SessionID const session_id,
    std::string& msg) {
  json root;
  root["type"] = "move_buffers_ownership_request";
  root["pid_to_id"] = pid_to_id;
  root["session_id"] = session_id;
  encode_msg(root, msg);
}

void WriteMoveBuffersOwnershipRequest(
    std::map<PlasmaID, PlasmaID> const& pid_to_pid, SessionID const session_id,
    std::string& msg) {
  json root;
  root["type"] = "move_buffers_ownership_request";
  root["pid_to_pid"] = pid_to_pid;
  root["session_id"] = session_id;
  encode_msg(root, msg);
}

Status ReadMoveBuffersOwnershipRequest(json const& root,
                                       std::map<ObjectID, ObjectID>& id_to_id,
                                       std::map<PlasmaID, ObjectID>& pid_to_id,
                                       std::map<ObjectID, PlasmaID>& id_to_pid,
                                       std::map<PlasmaID, PlasmaID>& pid_to_pid,
                                       SessionID& session_id) {
  RETURN_ON_ASSERT(root["type"] == "move_buffers_ownership_request");
  id_to_id = root.value<std::map<ObjectID, ObjectID>>("id_to_id", {});
  pid_to_id = root.value<std::map<PlasmaID, ObjectID>>("pid_to_id", {});
  id_to_pid = root.value<std::map<ObjectID, PlasmaID>>("id_to_pid", {});
  pid_to_pid = root.value<std::map<PlasmaID, PlasmaID>>("pid_to_pid", {});
  session_id = root["session_id"].get<SessionID>();
  return Status::OK();
}

void WriteMoveBuffersOwnershipReply(std::string& msg) {
  json root;
  root["type"] = "move_buffers_ownership_reply";
  encode_msg(root, msg);
}

Status ReadMoveBuffersOwnershipReply(json const& root) {
  CHECK_IPC_ERROR(root, "move_buffers_ownership_reply");
  return Status::OK();
}

void WriteReleaseRequest(ObjectID const& object_id, std::string& msg) {
  json root;
  root["type"] = "release_request";
  root["object_id"] = object_id;
  encode_msg(root, msg);
}

Status ReadReleaseRequest(json const& root, ObjectID& object_id) {
  RETURN_ON_ASSERT(root["type"] == "release_request");
  object_id = root["object_id"].get<ObjectID>();
  return Status::OK();
}

void WriteReleaseReply(std::string& msg) {
  json root;
  root["type"] = "release_reply";
  encode_msg(root, msg);
}

Status ReadReleaseReply(json const& root) {
  CHECK_IPC_ERROR(root, "release_reply");
  return Status::OK();
}

void WriteDelDataWithFeedbacksRequest(const std::vector<ObjectID>& id,
                                      const bool force, const bool deep,
                                      const bool fastpath, std::string& msg) {
  json root;
  root["type"] = "del_data_with_feedbacks_request";
  root["id"] = std::vector<ObjectID>{id};
  root["force"] = force;
  root["deep"] = deep;
  root["fastpath"] = fastpath;

  encode_msg(root, msg);
}

Status ReadDelDataWithFeedbacksRequest(json const& root,
                                       std::vector<ObjectID>& ids, bool& force,
                                       bool& deep, bool& fastpath) {
  RETURN_ON_ASSERT(root["type"] == "del_data_with_feedbacks_request");
  ids = root["id"].get_to(ids);
  force = root.value("force", false);
  deep = root.value("deep", false);
  fastpath = root.value("fastpath", false);
  return Status::OK();
}

void WriteDelDataWithFeedbacksReply(const std::vector<ObjectID>& deleted_bids,
                                    std::string& msg) {
  json root;
  root["type"] = "del_data_with_feedbacks_reply";
  root["deleted_bids"] = deleted_bids;

  encode_msg(root, msg);
}

Status ReadDelDataWithFeedbacksReply(json const& root,
                                     std::vector<ObjectID>& deleted_bids) {
  RETURN_ON_ASSERT(root["type"] == "del_data_with_feedbacks_reply");
  deleted_bids = root["deleted_bids"].get_to(deleted_bids);
  return Status::OK();
}

// IsInUse
void WriteIsInUseRequest(const ObjectID& id, std::string& msg) {
  json root;
  root["type"] = "is_in_use_request";
  root["id"] = id;
  encode_msg(root, msg);
}

Status ReadIsInUseRequest(json const& root, ObjectID& id) {
  RETURN_ON_ASSERT(root["type"] == "is_in_use_request");
  id = root["id"].get<ObjectID>();
  return Status::OK();
}

void WriteIsInUseReply(const bool is_in_use, std::string& msg) {
  json root;
  root["type"] = "is_in_use_reply";
  root["is_in_use"] = is_in_use;
  encode_msg(root, msg);
}

Status ReadIsInUseReply(json const& root, bool& is_in_use) {
  RETURN_ON_ASSERT(root["type"] == "is_in_use_reply");
  is_in_use = root["is_in_use"].get<bool>();
  return Status::OK();
}

// IsSpilled
void WriteIsSpilledRequest(const ObjectID& id, std::string& msg) {
  json root;
  root["type"] = "is_spilled_request";
  root["id"] = id;
  encode_msg(root, msg);
}

Status ReadIsSpilledRequest(json const& root, ObjectID& id) {
  RETURN_ON_ASSERT(root["type"] == "is_spilled_request");
  id = root["id"].get<ObjectID>();
  return Status::OK();
}

void WriteIsSpilledReply(const bool is_spilled, std::string& msg) {
  json root;
  root["type"] = "is_spilled_reply";
  root["is_spilled"] = is_spilled;
  encode_msg(root, msg);
}

Status ReadIsSpilledReply(json const& root, bool& is_spilled) {
  RETURN_ON_ASSERT(root["type"] == "is_spilled_reply");
  is_spilled = root["is_spilled"].get<bool>();
  return Status::OK();
}

void WriteIncreaseReferenceCountRequest(const std::vector<ObjectID>& ids,
                                        std::string& msg) {
  json root;
  root["type"] = "increase_reference_count_request";
  root["ids"] = std::vector<ObjectID>{ids};
  encode_msg(root, msg);
}

Status ReadIncreaseReferenceCountRequest(json const& root,
                                         std::vector<ObjectID>& ids) {
  RETURN_ON_ASSERT(root["type"] == "increase_reference_count_request");
  ids = root["ids"].get_to(ids);
  return Status::OK();
}

void WriteIncreaseReferenceCountReply(std::string& msg) {
  json root;
  root["type"] = "increase_reference_count_reply";
  encode_msg(root, msg);
}

Status ReadIncreaseReferenceCountReply(json const& root) {
  RETURN_ON_ASSERT(root["type"] == "increase_reference_count_reply");
  return Status::OK();
}

}  // namespace vineyard
