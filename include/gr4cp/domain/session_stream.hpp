#pragma once

#include <optional>
#include <string>
#include <vector>

#include "gr4cp/domain/session.hpp"

namespace gr4cp::domain {

struct BrowserStreamDescriptor {
    std::string id;
    std::string block_instance_name;
    std::string transport;
    std::string payload_format;
    std::string path;
    bool ready{false};
};

struct RuntimeStreamBindingPlan {
    std::string stream_id;
    std::string block_instance_name;
    std::string transport;
    std::string payload_format;
    std::string path;
    bool ready{false};
};

struct StreamRuntimePlan {
    std::vector<RuntimeStreamBindingPlan> streams;
};

struct InternalStreamBinding {
    std::string protocol;
    std::string host;
    int port{};
    std::string path;
    std::string endpoint;
};

struct RuntimeStreamBinding {
    RuntimeStreamBindingPlan plan;
    InternalStreamBinding internal;
    BrowserStreamDescriptor browser;
};

std::optional<StreamRuntimePlan> derive_stream_runtime_plan(const Session& session);
std::vector<std::optional<StreamRuntimePlan>> derive_stream_runtime_plans(const std::vector<Session>& sessions);
BrowserStreamDescriptor to_browser_stream_descriptor(const RuntimeStreamBindingPlan& plan);
std::optional<std::string> validate_stream_runtime_contract(const Session& session);

}  // namespace gr4cp::domain
