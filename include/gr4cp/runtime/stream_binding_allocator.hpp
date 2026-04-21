#pragma once

#include "gr4cp/domain/session_stream.hpp"

#include <mutex>
#include <set>
#include <string>

namespace gr4cp::runtime {

class StreamBindingAllocator {
public:
    domain::InternalStreamBinding allocate_http();
    domain::InternalStreamBinding allocate_websocket();
    void release(const domain::InternalStreamBinding& binding);

private:
    std::mutex mutex_;
    std::set<int> allocated_ports_;
};

}  // namespace gr4cp::runtime
