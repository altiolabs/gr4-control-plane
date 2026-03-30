#pragma once

#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "gr4cp/catalog/block_catalog_provider.hpp"
#include "gr4cp/domain/block_catalog.hpp"

namespace gr4cp::app {

class BlockCatalogService {
public:
    explicit BlockCatalogService(const catalog::BlockCatalogProvider& provider);

    std::vector<domain::BlockDescriptor> list() const;
    domain::BlockDescriptor get(const std::string& id) const;

private:
    struct CachedCatalog {
        std::vector<domain::BlockDescriptor> browse_blocks;
        std::unordered_map<std::string, domain::BlockDescriptor> exact_blocks;
    };

    const CachedCatalog& cached_catalog() const;

    const catalog::BlockCatalogProvider& provider_;
    mutable std::mutex mutex_;
    mutable std::optional<CachedCatalog> cached_catalog_;
};

}  // namespace gr4cp::app
