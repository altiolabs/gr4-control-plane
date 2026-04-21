#include "gr4cp/app/session_stream_service.hpp"

namespace gr4cp::app {

std::optional<domain::StreamRuntimePlan> SessionStreamService::derive_plan(const domain::Session& session) const {
    return domain::derive_stream_runtime_plan(session);
}

std::vector<std::optional<domain::StreamRuntimePlan>> SessionStreamService::derive_plans(
    const std::vector<domain::Session>& sessions) const {
    return domain::derive_stream_runtime_plans(sessions);
}

}  // namespace gr4cp::app
