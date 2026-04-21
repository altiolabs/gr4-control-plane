#pragma once

#include <optional>
#include <vector>

#include "gr4cp/domain/session.hpp"
#include "gr4cp/domain/session_stream.hpp"

namespace gr4cp::app {

class SessionStreamService {
public:
    std::optional<domain::StreamRuntimePlan> derive_plan(const domain::Session& session) const;
    std::vector<std::optional<domain::StreamRuntimePlan>> derive_plans(const std::vector<domain::Session>& sessions) const;
};

}  // namespace gr4cp::app
