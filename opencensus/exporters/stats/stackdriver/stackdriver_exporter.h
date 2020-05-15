// Copyright 2018, OpenCensus Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENCENSUS_EXPORTERS_STATS_STACKDRIVER_STACKDRIVER_EXPORTER_H_
#define OPENCENSUS_EXPORTERS_STATS_STACKDRIVER_STACKDRIVER_EXPORTER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "google/api/monitored_resource.pb.h"
#include "google/monitoring/v3/metric_service.grpc.pb.h"
#include "opencensus/stats/stats.h"

namespace opencensus {
namespace exporters {
namespace stats {

struct StackdriverOptions {
  // The Stackdriver project ID to use.
  std::string project_id;

  // Optional: The opencensus_task is used to uniquely identify the task in
  // Stackdriver. If empty, the exporter will use "cpp-{PID}@{HOSTNAME}" by
  // default.
  std::string opencensus_task;

  // The RPC deadline to use when exporting to Stackdriver.
  absl::Duration rpc_deadline = absl::Seconds(60);

  // Optional: the Stackdriver MonitoredResource to use.
  //
  // If not set (i.e. if monitored_resource.type is empty), the exporter will
  // use the "global" resource.
  //
  // See also:
  // https://cloud.google.com/monitoring/api/ref_v3/rpc/google.api#google.api.MonitoredResource
  // https://cloud.google.com/monitoring/api/resources
  google::api::MonitoredResource monitored_resource;

  // Optional: per metric Stackdriver MonitoredResource to use. Key is the view
  // descriptor name, value is the monitored resource to use for that view.
  //
  // If the view name cannot be found in the map, the exporter will use the
  // monitored_resource set above, or the "global" resource if that is not
  // set.
  //
  // See also:
  // https://cloud.google.com/monitoring/api/ref_v3/rpc/google.api#google.api.MonitoredResource
  // https://cloud.google.com/monitoring/api/resources
  std::unordered_map<std::string, google::api::MonitoredResource>
      per_metric_monitored_resource;

  // Optional: the name prefix for Stackdriver metrics.
  //
  // It is suggested to use a prefix with a custom or external domain name, for
  // example:
  //   - "custom.googleapis.com/myorg/"
  //   - "external.googleapis.com/prometheus/"
  //
  // If empty, the exporter will use "custom.googleapis.com/opencensus/" by
  // default.
  std::string metric_name_prefix;

  // Optional: by default, the exporter connects to Stackdriver using gRPC. If
  // this stub is non-null, the exporter will use this stub to send gRPC calls
  // instead. Useful for testing.
  std::unique_ptr<google::monitoring::v3::MetricService::StubInterface>
      metric_service_stub;
};

// Exports stats for registered views (see opencensus/stats/stats_exporter.h) to
// Stackdriver. StackdriverExporter is thread-safe.
class StackdriverExporter {
 public:
  // Registers the exporter.
  static void Register(StackdriverOptions&& opts);

  // Registers the exporter. Takes ownership of opts.metric_service_stub
  // and resets it to nullptr.
  ABSL_DEPRECATED(
      "Register() without rvalue StackdriverOptions is deprecated and "
      "will be removed on or after 2020-01-18")
  static void Register(StackdriverOptions& opts);

  // TODO: Retire this:
  ABSL_DEPRECATED(
      "Register() without StackdriverOptions is deprecated and "
      "will be removed on or after 2019-03-20")
  static void Register(absl::string_view project_id,
                       absl::string_view opencensus_task);

 private:
  StackdriverExporter() = delete;
};

class StackdriverExporterHandler :
    public ::opencensus::stats::StatsExporter::Handler {
 public:
  explicit StackdriverExporterHandler(StackdriverOptions&& opts);

  void ExportViewData(
      const std::vector<std::pair<opencensus::stats::ViewDescriptor,
                                  opencensus::stats::ViewData>>& data)
      ABSL_LOCKS_EXCLUDED(mu_) override;

 protected:
  // Allows subclasses to provide custom logic to set the ClientContext
  // such as setting grpc::CallCredentials.
  virtual void PrepareClientContext(grpc::ClientContext* context);

 private:
  // Registers 'descriptor' with Stackdriver if no view by that name has been
  // registered by this, and adds it to registered_descriptors_ if successful.
  // Returns true if the view has already been registered or registration is
  // successful, and false if the registration fails or the name has already
  // been registered with different parameters.
  bool MaybeRegisterView(const opencensus::stats::ViewDescriptor& descriptor,
                         bool add_task_label)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  const StackdriverOptions opts_;
  mutable absl::Mutex mu_;
  std::unordered_map<std::string, opencensus::stats::ViewDescriptor>
      registered_descriptors_ ABSL_GUARDED_BY(mu_);
};

}  // namespace stats
}  // namespace exporters
}  // namespace opencensus

#endif  // OPENCENSUS_EXPORTERS_STATS_STACKDRIVER_STACKDRIVER_EXPORTER_H_
