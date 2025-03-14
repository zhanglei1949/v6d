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

#include "server/server/vineyard_server.h"

#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "common/util/callback.h"
#include "common/util/json.h"
#include "common/util/logging.h"
#include "server/async/ipc_server.h"
#include "server/async/rpc_server.h"
#include "server/services/meta_service.h"
#include "server/util/kubectl.h"
#include "server/util/meta_tree.h"
#include "server/util/metrics.h"
#include "server/util/proc.h"

namespace vineyard {

#ifndef ENSURE_VINEYARDD_READY
#define ENSURE_VINEYARDD_READY()                       \
  do {                                                 \
    if (!((ready_ & kMeta) && (ready_ & kBulk))) {     \
      std::stringstream ss;                            \
      ss << "{";                                       \
      ss << "meta: " << (ready_ & kMeta) << ", ";      \
      ss << "bulk store: " << (ready_ & kBulk);        \
      ss << "}";                                       \
      return Status::VineyardServerNotReady(ss.str()); \
    }                                                  \
  } while (0)
#endif  // ENSURE_VINEYARDD_READY

bool DeferredReq::Alive() const { return alive_fn_(); }

bool DeferredReq::TestThenCall(const json& meta) const {
  if (test_fn_(meta)) {
    VINEYARD_SUPPRESS(call_fn_(meta));
    return true;
  }
  return false;
}

VineyardServer::VineyardServer(const json& spec, const SessionID& session_id,
                               std::shared_ptr<VineyardRunner> runner,
                               asio::io_context& context,
                               asio::io_context& meta_context,
                               asio::io_context& io_context,
                               callback_t<std::string const&> callback)
    : spec_(spec),
      session_id_(session_id),
      context_(context),
      meta_context_(meta_context),
      io_context_(io_context),
      callback_(callback),
      runner_(runner),
      ready_(0) {}

template <>
std::shared_ptr<BulkStore> VineyardServer::GetBulkStore<ObjectID>() {
  return bulk_store_;
}

template <>
std::shared_ptr<PlasmaBulkStore> VineyardServer::GetBulkStore<PlasmaID>() {
  return plasma_bulk_store_;
}

Status VineyardServer::Serve(StoreType const& bulk_store_type) {
  stopped_.store(false);
  this->bulk_store_type_ = bulk_store_type;

  // Initialize the ipc/rpc server ptr first to get self endpoints when
  // initializing the metadata service.
  ipc_server_ptr_ = std::make_shared<IPCServer>(shared_from_this());
  if (session_id_ == RootSessionID() && spec_["rpc_spec"]["rpc"].get<bool>()) {
    // TODO: the rpc won't be enabled for child sessions as we are unsure
    // about how to select the port.
    rpc_server_ptr_ = std::make_shared<RPCServer>(shared_from_this());
  }

  this->meta_service_ptr_ = IMetaService::Get(shared_from_this());
  RETURN_ON_ERROR(this->meta_service_ptr_->Start());

  auto memory_limit = spec_["bulkstore_spec"]["memory_size"].get<size_t>();
  auto allocator = spec_["bulkstore_spec"]["allocator"].get<std::string>();

  if (bulk_store_type_ == StoreType::kPlasma) {
    plasma_bulk_store_ = std::make_shared<PlasmaBulkStore>();
    RETURN_ON_ERROR(plasma_bulk_store_->PreAllocate(memory_limit, allocator));

    // TODO(mengke.mk): Currently we do not allow streamming in plasma
    // bulkstore, anyway, we can templatize stream store to solve this.
    stream_store_ = nullptr;
  } else if (bulk_store_type_ == StoreType::kDefault) {
    bulk_store_ = std::make_shared<BulkStore>();
    auto spill_lower_bound_rate =
        spec_["bulkstore_spec"]["spill_lower_bound_rate"].get<double>();
    auto spill_upper_bound_rate =
        spec_["bulkstore_spec"]["spill_upper_bound_rate"].get<double>();
    RETURN_ON_ERROR(bulk_store_->PreAllocate(memory_limit, allocator));

    // setup spill
    bulk_store_->SetMemSpillUpBound(memory_limit * spill_upper_bound_rate);
    bulk_store_->SetMemSpillLowBound(memory_limit * spill_lower_bound_rate);
    bulk_store_->SetSpillPath(
        spec_["bulkstore_spec"]["spill_path"].get<std::string>());

    // setup stream store
    stream_store_ = std::make_shared<StreamStore>(
        shared_from_this(), bulk_store_,
        spec_["bulkstore_spec"]["stream_threshold"].get<size_t>());
  }

  BulkReady();

  serve_status_ = Status::OK();
  return serve_status_;
}

Status VineyardServer::Finalize() { return Status::OK(); }

void VineyardServer::Ready() {
  VINEYARD_DISCARD(callback_(Status::OK(), IPCSocket()));
}

void VineyardServer::BackendReady() {
  try {
    if (ipc_server_ptr_) {
      ipc_server_ptr_->Start();
      LOG_SUMMARY("ipc_connection_total", this->instance_id(), 1);
    }
  } catch (std::exception const& ex) {
    LOG(ERROR) << "Failed to start vineyard IPC server: " << ex.what()
               << ", or please try to cleanup existing "
               << spec_["ipc_spec"]["socket"];
    serve_status_ = Status::IOError();
    VINEYARD_DISCARD(callback_(serve_status_, IPCSocket()));
    context_.stop();
    return;
  }

  try {
    if (rpc_server_ptr_) {
      rpc_server_ptr_->Start();
      LOG_SUMMARY("rpc_connection_total", this->instance_id(), 1);
    } else {
      RPCReady();
    }
  } catch (std::exception const& ex) {
    LOG(ERROR) << "Failed to start vineyard RPC server: " << ex.what();
    serve_status_ = Status::IOError();
    VINEYARD_DISCARD(callback_(serve_status_, IPCSocket()));
    context_.stop();
    return;
  }
}

void VineyardServer::MetaReady() {
  VINEYARD_ASSERT(!(ready_ & kMeta), "A component can't be initialized twice!");
  ready_ |= kMeta;
  if (ready_ == kReady) {
    Ready();
  }
  if (ready_ == kBackendReady) {
    BackendReady();
  }
}

void VineyardServer::BulkReady() {
  VINEYARD_ASSERT(!(ready_ & kBulk), "A component can't be initialized twice!");
  ready_ |= kBulk;
  if (ready_ == kReady) {
    Ready();
  }
  if (ready_ == kBackendReady) {
    BackendReady();
  }
}

void VineyardServer::IPCReady() {
  VINEYARD_ASSERT(!(ready_ & kIPC), "A component can't be initialized twice!");
  ready_ |= kIPC;
  if (ready_ == kReady) {
    Ready();
  }
}

void VineyardServer::RPCReady() {
  VINEYARD_ASSERT(!(ready_ & kRPC), "A component can't be initialized twice!");
  ready_ |= kRPC;
  if (ready_ == kReady) {
    Ready();
  }
}

Status VineyardServer::GetData(const std::vector<ObjectID>& ids,
                               const bool sync_remote, const bool wait,
                               std::function<bool()> alive,
                               callback_t<const json&> callback) {
  ENSURE_VINEYARDD_READY();
  meta_service_ptr_->RequestToGetData(
      sync_remote, [this, ids, wait, alive, callback](const Status& status,
                                                      const json& meta) {
        if (status.ok()) {
      // When object not exists, we return an empty json, rather than
      // the status to indicate the error.
#if !defined(NDEBUG)
          if (VLOG_IS_ON(100)) {
            DVLOG(100) << "Got request from client to get data, dump json:";
            std::cerr << meta.dump(4) << std::endl;
            DVLOG(100) << "=========================================";
            std::stringstream ss;
            for (auto const& id : ids) {
              ss << id << "(" << ObjectIDToString(id) << "), ";
            }
            DVLOG(100) << "Requesting objects: " << ss.str();
            DVLOG(100) << "=========================================";
          }
#endif
          auto test_task = [this, ids](const json& meta) -> bool {
            for (auto const& id : ids) {
              bool exists = false;
              if (IsBlob(id)) {
                exists = this->bulk_store_->Exists(id);
              } else {
                Status status;
                CATCH_JSON_ERROR(status, meta_tree::Exists(meta, id, exists));
                VINEYARD_SUPPRESS(status);
              }
              if (!exists) {
                return exists;
              }
            }
            return true;
          };
          auto eval_task = [this, ids, callback](const json& meta) -> Status {
            json sub_tree_group;
            for (auto const& id : ids) {
              json sub_tree;
              if (IsBlob(id)) {
                std::shared_ptr<Payload> object;
                auto status = this->bulk_store_->Get(id, object);
                if (status.ok()) {
                  sub_tree["id"] = ObjectIDToString(id);
                  sub_tree["typename"] = "vineyard::Blob";
                  sub_tree["length"] = object->data_size;
                  sub_tree["nbytes"] = object->data_size;
                  sub_tree["transient"] = true;
                  sub_tree["instance_id"] = this->instance_id();
                } else {
                  VLOG(10) << "Failed to find payload for blob: "
                           << ObjectIDToString(id)
                           << ", reason: " << status.ToString();
                }
              } else {
                Status s;
                CATCH_JSON_ERROR(
                    s, meta_tree::GetData(meta, this->instance_name(), id,
                                          sub_tree, instance_id_));
                if (s.IsMetaTreeInvalid()) {
                  LOG(WARNING) << "Found errors in metadata: " << s.ToString();
                }
#if !defined(NDEBUG)
                if (VLOG_IS_ON(100)) {
                  DVLOG(100) << "Got request response:";
                  std::cerr << sub_tree.dump(4) << std::endl;
                  DVLOG(100) << "=========================================";
                }
#endif
              }
              if (sub_tree.is_object() && !sub_tree.empty()) {
                sub_tree_group[ObjectIDToString(id)] = sub_tree;
              }
            }
            return callback(Status::OK(), sub_tree_group);
          };
          if (!wait || test_task(meta)) {
            return eval_task(meta);
          } else {
            this->deferred_.emplace_back(alive, test_task, eval_task);
            return Status::OK();
          }
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      });
  return Status::OK();
}

Status VineyardServer::ListData(std::string const& pattern, bool const regex,
                                size_t const limit,
                                callback_t<const json&> callback) {
  ENSURE_VINEYARDD_READY();
  meta_service_ptr_->RequestToGetData(
      false,  // no need for sync from etcd
      [this, pattern, regex, limit, callback](const Status& status,
                                              const json& meta) {
        if (status.ok()) {
          json sub_tree_group;
          Status s;
          CATCH_JSON_ERROR(
              s, meta_tree::ListData(meta, this->instance_name(), pattern,
                                     regex, limit, sub_tree_group));
          if (!s.ok()) {
            return callback(s, sub_tree_group);
          }
          size_t current = sub_tree_group.size();
          if (current < limit &&
              meta_tree::MatchTypeName(false, pattern, "vineyard::Blob")) {
            // consider returns blob when not reach the limit
            auto const& blobs = bulk_store_->List();
            for (auto const& item : blobs) {
              if (current >= limit) {
                break;
              }
              if (!item.second->IsSealed()) {
                // skip unsealed blobs, otherwise `GetBuffers()` will fail on
                // client after `ListData()`.
                continue;
              }
              if (item.first ==
                  GenerateBlobID(std::numeric_limits<uintptr_t>::max())) {
                // skip the dummy blob with the initialized blob id
                continue;
              }
              std::string sub_tree_key = ObjectIDToString(item.first);
              json sub_tree;
              {
                sub_tree["id"] = sub_tree_key;
                sub_tree["typename"] = "vineyard::Blob";
                sub_tree["length"] = item.second->data_size;
                sub_tree["nbytes"] = item.second->data_size;
                sub_tree["transient"] = true;
                sub_tree["instance_id"] = this->instance_id();
              }
              sub_tree_group[sub_tree_key] = sub_tree;
              current += 1;
            }
          }
          return callback(status, sub_tree_group);
        } else {
          LOG(ERROR) << status.ToString();
          return callback(status, json{});
        }
      });
  return Status::OK();
}

Status VineyardServer::ListAllData(
    callback_t<std::vector<ObjectID> const&> callback) {
  ENSURE_VINEYARDD_READY();
  meta_service_ptr_->RequestToGetData(
      false,  // no need for sync from etcd
      [this, callback](const Status& status, const json& meta) {
        if (status.ok()) {
          std::vector<ObjectID> objects;
          Status s;
          CATCH_JSON_ERROR(s, meta_tree::ListAllData(meta, objects));
          if (!s.ok()) {
            return callback(s, objects);
          }
          auto const& blobs = bulk_store_->List();
          for (auto const& item : blobs) {
            objects.emplace_back(item.first);
          }
          return callback(status, objects);
        } else {
          LOG(ERROR) << status.ToString();
          return callback(status, {});
        }
      });
  return Status::OK();
}

Status VineyardServer::CreateData(
    const json& tree,
    callback_t<const ObjectID, const Signature, const InstanceID> callback) {
  ENSURE_VINEYARDD_READY();
  ObjectID id = GenerateObjectID();
#if !defined(NDEBUG)
  if (VLOG_IS_ON(100)) {
    DVLOG(100) << "Got request from client to create data:";
    // NB: glog has limit on maximum lines.
    std::cerr << id << " " << ObjectIDToString(id) << " " << tree.dump(4)
              << std::endl;
    DVLOG(100) << "=========================================";
  }
#endif
  // validate typename
  auto type_name_node = tree.value("typename", json(nullptr));
  if (type_name_node.is_null() || !type_name_node.is_string()) {
    VINEYARD_DISCARD(callback(Status::MetaTreeInvalid("No typename field"),
                              InvalidObjectID(), InvalidSignature(),
                              UnspecifiedInstanceID()));
    return Status::OK();
  }
  std::string const& type = type_name_node.get_ref<std::string const&>();

  RETURN_ON_ASSERT(type != "vineyard::Blob", "Blob has no metadata");

  // Check if instance_id information available
  RETURN_ON_ASSERT(tree.contains("instance_id"),
                   "The instance_id filed must be presented");

  auto decorated_tree = tree;
  Signature signature = GenerateSignature();
  if (decorated_tree.find("signature") != decorated_tree.end()) {
    signature = decorated_tree["signature"].get<Signature>();
  } else {
    decorated_tree["signature"] = signature;
  }

  // update meta into json
  meta_service_ptr_->RequestToBulkUpdate(
      [this, id, decorated_tree](const Status& status, const json& meta,
                                 std::vector<meta_tree::op_t>& ops,
                                 InstanceID& computed_instance_id) {
        if (status.ok()) {
          Status s;
          CATCH_JSON_ERROR(s, meta_tree::PutDataOps(meta, this->instance_name(),
                                                    id, decorated_tree, ops,
                                                    computed_instance_id));
          return s;
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      },
      boost::bind(callback, _1, id, signature, _2));
  return Status::OK();
}

Status VineyardServer::Persist(const ObjectID id, callback_t<> callback) {
  ENSURE_VINEYARDD_READY();
  RETURN_ON_ASSERT(!IsBlob(id), "The blobs cannot be persisted");
  meta_service_ptr_->RequestToPersist(
      [this, id](const Status& status, const json& meta,
                 std::vector<meta_tree::op_t>& ops) {
        if (status.ok()) {
          Status s;
          CATCH_JSON_ERROR(
              s, meta_tree::PersistOps(meta, this->instance_name(), id, ops));
          if (status.ok() && !ops.empty() &&
              this->spec_["sync_crds"].get<bool>()) {
            json tree;
            Status s;
            CATCH_JSON_ERROR(
                s, meta_tree::GetData(meta, this->instance_name(), id, tree));
            VINEYARD_SUPPRESS(s);
            if (tree.is_object() && !tree.empty()) {
              auto kube = std::make_shared<Kubectl>(this->GetMetaContext());
              kube->ApplyObject(meta["instances"], tree);
              kube->Finish();
            }
          }
          return s;
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      },
      callback);
  return Status::OK();
}

Status VineyardServer::IfPersist(const ObjectID id,
                                 callback_t<const bool> callback) {
  ENSURE_VINEYARDD_READY();
  // How to decide if an object (an id) is persist:
  //
  // 1. every object has a `persist` field in meta
  // 2. if the object has been persist by other client that connects to
  //    the same vineyardd: just read the meta
  // 3. if the object has been persist on other vineyardd: that is impossible,
  //    since we cannot get a remote object before it has been persisted.
  //
  // Thus we just need to read from the metadata in vineyardd, without
  // touching etcd.
  if (IsBlob(id)) {
    context_.post(boost::bind(callback, Status::OK(), false));
    return Status::OK();
  }
  meta_service_ptr_->RequestToGetData(
      false, [id, callback](const Status& status, const json& meta) {
        if (status.ok()) {
          bool persist = false;
          Status s;
          CATCH_JSON_ERROR(s, meta_tree::IfPersist(meta, id, persist));
          return callback(s, persist);
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      });
  return Status::OK();
}

Status VineyardServer::Exists(const ObjectID id,
                              callback_t<const bool> callback) {
  ENSURE_VINEYARDD_READY();
  if (IsBlob(id)) {
    context_.post([this, id, callback] {
      VINEYARD_DISCARD(callback(Status::OK(), bulk_store_->Exists(id)));
    });
    return Status::OK();
  }
  meta_service_ptr_->RequestToGetData(
      true, [id, callback](const Status& status, const json& meta) {
        if (status.ok()) {
          bool exists = false;
          Status s;
          CATCH_JSON_ERROR(s, meta_tree::Exists(meta, id, exists));
          return callback(s, exists);
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      });
  return Status::OK();
}

Status VineyardServer::ShallowCopy(const ObjectID id,
                                   const json& extra_metadata,
                                   callback_t<const ObjectID> callback) {
  ENSURE_VINEYARDD_READY();
  RETURN_ON_ASSERT(!IsBlob(id), "The blobs cannot be shallow copied");
  ObjectID target_id = GenerateObjectID();
  meta_service_ptr_->RequestToShallowCopy(
      [id, extra_metadata, target_id](const Status& status, const json& meta,
                                      std::vector<meta_tree::op_t>& ops,
                                      bool& transient) {
        if (status.ok()) {
          Status s;

          CATCH_JSON_ERROR(
              s, meta_tree::ShallowCopyOps(meta, id, extra_metadata, target_id,
                                           ops, transient));
          return s;
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      },
      [target_id, callback](const Status& status) {
        return callback(status, target_id);
      });
  return Status::OK();
}

Status VineyardServer::DelData(const std::vector<ObjectID>& ids,
                               const bool force, const bool deep,
                               const bool fastpath, callback_t<> callback) {
  return DelData(ids, force, deep, fastpath,
                 [callback](Status const& status,
                            std::vector<ObjectID> const& deleted_ids) {
                   return callback(status);
                 });
}

Status VineyardServer::DelData(
    const std::vector<ObjectID>& ids, const bool force, const bool deep,
    const bool fastpath, callback_t<std::vector<ObjectID> const&> callback) {
  ENSURE_VINEYARDD_READY();
  if (fastpath) {
    // forcely delete the given blobs: used for allocators
    for (auto const id : ids) {
      RETURN_ON_ASSERT(IsBlob(id),
                       "Fastpath deletion can only be applied to blobs");
    }
    context_.post([this, ids, callback] {
      for (auto const id : ids) {
        VINEYARD_DISCARD(bulk_store_->OnDelete(id));
      }
      VINEYARD_DISCARD(callback(Status::OK(), ids));
    });
    return Status::OK();
  }
  meta_service_ptr_->RequestToDelete(
      ids, force, deep,
      [](const Status& status, const json& meta,
         std::vector<ObjectID> const& ids_to_delete,
         std::vector<meta_tree::op_t>& ops, bool& sync_remote) {
        if (status.ok()) {
          Status s;
          CATCH_JSON_ERROR(
              s, meta_tree::DelDataOps(meta, ids_to_delete, ops, sync_remote));
          if (s.IsMetaTreeSubtreeNotExists()) {
            return Status::ObjectNotExists("failed to delete: " + s.ToString());
          }
          return s;
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      },
      callback);
  return Status::OK();
}

Status VineyardServer::DeleteBlobBatch(const std::set<ObjectID>& ids) {
  for (auto object_id : ids) {
    VINEYARD_SUPPRESS(this->bulk_store_->OnDelete(object_id));
  }
  return Status::OK();
}

Status VineyardServer::DeleteAllAt(const json& meta,
                                   InstanceID const instance_id) {
  std::vector<ObjectID> objects_to_cleanup;
  Status status;
  CATCH_JSON_ERROR(status, meta_tree::FilterAtInstance(meta, instance_id,
                                                       objects_to_cleanup));
  RETURN_ON_ERROR(status);
  return this->DelData(objects_to_cleanup, true, true, false /* fastpath */,
                       [](Status const& status) -> Status {
                         if (!status.ok()) {
                           LOG(ERROR) << "Error happens on cleanup: "
                                      << status.ToString();
                         }
                         return Status::OK();
                       });
}

Status VineyardServer::PutName(const ObjectID object_id,
                               const std::string& name, callback_t<> callback) {
  ENSURE_VINEYARDD_READY();
  meta_service_ptr_->RequestToPersist(
      [object_id, name](const Status& status, const json& meta,
                        std::vector<meta_tree::op_t>& ops) {
        if (status.ok()) {
          // TODO: do proper validation:
          // 1. global objects can have name, local ones cannot.
          // 2. the name-object_id mapping shouldn't be overwrite.

          // blob cannot have name
          if (IsBlob(object_id)) {
            return Status::Invalid("blobs cannot have name");
          }

          bool exists = false;
          {
            Status s;
            CATCH_JSON_ERROR(s, meta_tree::Exists(meta, object_id, exists));
            VINEYARD_DISCARD(s);
          }
          if (!exists) {
            return Status::ObjectNotExists("failed to put name: object " +
                                           ObjectIDToString(object_id) +
                                           " doesn't exist");
          }

          bool persist = false;
          {
            Status s;
            CATCH_JSON_ERROR(s, meta_tree::IfPersist(meta, object_id, persist));
            VINEYARD_DISCARD(s);
          }
          if (!persist) {
            return Status::Invalid(
                "transient objects cannot have name, please persist it first");
          }

          ops.emplace_back(meta_tree::op_t::Put("/names/" + name, object_id));
          ops.emplace_back(meta_tree::op_t::Put(
              "/data/" + ObjectIDToString(object_id) + "/__name",
              meta_tree::EncodeValue(name)));
          return Status::OK();
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      },
      callback);
  return Status::OK();
}

Status VineyardServer::GetName(const std::string& name, const bool wait,
                               DeferredReq::alive_t alive,
                               callback_t<const ObjectID&> callback) {
  ENSURE_VINEYARDD_READY();
  meta_service_ptr_->RequestToGetData(true, [this, name, wait, alive, callback](
                                                const Status& status,
                                                const json& meta) {
    if (status.ok()) {
      auto test_task = [name](const json& meta) -> bool {
        auto names = meta.value("names", json(nullptr));
        if (names.is_object()) {
          return names.contains(name);
        }
        return false;
      };
      auto eval_task = [name, callback](const json& meta) -> Status {
        auto names = meta.value("names", json(nullptr));
        if (names.is_object() && names.contains(name)) {
          auto entry = names[name];
          if (!entry.is_null()) {
            return callback(Status::OK(), entry.get<ObjectID>());
          }
        }
        return callback(Status::ObjectNotExists("failed to find name: " + name),
                        InvalidObjectID());
      };
      if (!wait || test_task(meta)) {
        return eval_task(meta);
      } else {
        deferred_.emplace_back(alive, test_task, eval_task);
        return Status::OK();
      }
    } else {
      LOG(ERROR) << status.ToString();
      return status;
    }
  });
  return Status::OK();
}

Status VineyardServer::DropName(const std::string& name,
                                callback_t<> callback) {
  ENSURE_VINEYARDD_READY();
  meta_service_ptr_->RequestToPersist(
      [name](const Status& status, const json& meta,
             std::vector<meta_tree::op_t>& ops) {
        if (status.ok()) {
          auto names = meta.value("names", json(nullptr));
          if (names.is_object()) {
            auto iter = names.find(name);
            if (iter != names.end()) {
              ops.emplace_back(meta_tree::op_t::Del("/names/" + name));
              auto object_id = iter->get<ObjectID>();
              // delete the name in the object meta as well.
              bool exists = false;
              {
                Status s;
                CATCH_JSON_ERROR(s, meta_tree::Exists(meta, object_id, exists));
                VINEYARD_DISCARD(s);
              }

              if (exists) {
                ops.emplace_back(meta_tree::op_t::Del(
                    "/data/" + ObjectIDToString(object_id) + "/__name"));
              }
            }
          }
          return Status::OK();
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      },
      callback);
  return Status::OK();
}

Status VineyardServer::ClusterInfo(callback_t<const json&> callback) {
  ENSURE_VINEYARDD_READY();
  meta_service_ptr_->RequestToGetData(
      true, [callback](const Status& status, const json& meta) {
        if (status.ok()) {
          return callback(status, meta["instances"]);
        } else {
          LOG(ERROR) << status.ToString();
          return status;
        }
      });
  return Status::OK();
}

Status VineyardServer::InstanceStatus(callback_t<const json&> callback) {
  ENSURE_VINEYARDD_READY();

  json status;
  status["instance_id"] = instance_id_;
  status["deployment"] = GetDeployment();
  status["memory_usage"] = bulk_store_->Footprint();
  status["memory_limit"] = bulk_store_->FootprintLimit();
  status["deferred_requests"] = deferred_.size();
  if (ipc_server_ptr_) {
    status["ipc_connections"] = ipc_server_ptr_->AliveConnections();
  } else {
    status["ipc_connections"] = 0;
  }
  if (rpc_server_ptr_) {
    status["rpc_connections"] = rpc_server_ptr_->AliveConnections();
  } else {
    status["rpc_connections"] = 0;
  }

  return callback(Status::OK(), status);
}

Status VineyardServer::ProcessDeferred(const json& meta) {
  auto iter = deferred_.begin();
  while (iter != deferred_.end()) {
    if (!iter->Alive() || iter->TestThenCall(meta)) {
      deferred_.erase(iter++);
    } else {
      ++iter;
    }
  }
  return Status::OK();
}

const std::string VineyardServer::IPCSocket() {
  if (this->ipc_server_ptr_) {
    return ipc_server_ptr_->Socket();
  } else {
    return "-";
  }
}

const std::string VineyardServer::RPCEndpoint() {
  if (this->rpc_server_ptr_) {
    return rpc_server_ptr_->Endpoint();
  } else {
    return "0.0.0.0:0";
  }
}

void VineyardServer::Stop() {
  if (stopped_.exchange(true)) {
    return;
  }

  if (this->ipc_server_ptr_) {
    this->ipc_server_ptr_->Stop();
  }
  if (this->rpc_server_ptr_) {
    this->rpc_server_ptr_->Stop();
  }
  if (this->meta_service_ptr_) {
    this->meta_service_ptr_->Stop();
  }

  // cleanup
  this->ipc_server_ptr_.reset();
  this->rpc_server_ptr_.reset();
  this->meta_service_ptr_.reset();
  this->stream_store_.reset();
  this->bulk_store_.reset();
  this->plasma_bulk_store_.reset();
}

bool VineyardServer::Running() const { return !stopped_.load(); }

VineyardServer::~VineyardServer() { this->Stop(); }

}  // namespace vineyard
