#pragma once

#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "gr4cp/domain/session.hpp"
#include "gr4cp/runtime/runtime_manager.hpp"
#include "gr4cp/storage/session_repository.hpp"

namespace gr4cp::app {

class NotFoundError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class InvalidStateError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class RuntimeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class SessionService {
public:
    SessionService(storage::SessionRepository& repository, runtime::RuntimeManager& runtime_manager);

    domain::Session create(const std::string& name, const std::string& grc);
    std::vector<domain::Session> list();
    domain::Session get(const std::string& id);
    domain::Session start(const std::string& id);
    domain::Session stop(const std::string& id);
    domain::Session restart(const std::string& id);
    void remove(const std::string& id);
    std::optional<domain::StreamRuntimePlan> active_stream_plan(const std::string& id);
    runtime::HttpStreamResponse fetch_http_stream(const std::string& id, const std::string& stream_id);
    runtime::WebSocketStreamRoute resolve_websocket_stream(const std::string& id, const std::string& stream_id);

private:
    enum class InternalLifecyclePhase {
        Idle,
        Starting,
        Running,
        Stopping,
        Failed,
    };

    InternalLifecyclePhase phase_for_locked(const domain::Session& session);
    domain::Session load_or_throw(const std::string& id);
    void store_session_locked(const domain::Session& session, InternalLifecyclePhase phase);
    void erase_phase_locked(const std::string& id);
    [[noreturn]] void record_runtime_failure(domain::Session session, const std::string& message);

    storage::SessionRepository& repository_;
    runtime::RuntimeManager& runtime_manager_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, InternalLifecyclePhase> phases_;
};

}  // namespace gr4cp::app
