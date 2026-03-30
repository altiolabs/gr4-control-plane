#include "gr4cp/app/block_catalog_service.hpp"

#include <algorithm>
#include <unordered_set>

#include "gr4cp/app/session_service.hpp"

namespace gr4cp::app {

BlockCatalogService::BlockCatalogService(const catalog::BlockCatalogProvider& provider) : provider_(provider) {}

std::vector<domain::BlockDescriptor> BlockCatalogService::list() const {
    std::lock_guard lock(mutex_);
    return cached_catalog().browse_blocks;
}

domain::BlockDescriptor BlockCatalogService::get(const std::string& id) const {
    std::lock_guard lock(mutex_);
    const auto& catalog = cached_catalog();
    const auto it = catalog.exact_blocks.find(id);
    if (it == catalog.exact_blocks.end()) {
        throw NotFoundError("block not found: " + id);
    }
    return it->second;
}

namespace {

std::string canonical_catalog_key(const domain::BlockDescriptor& block) {
    if (block.canonical_type.has_value() && !block.canonical_type->empty()) {
        return *block.canonical_type;
    }
    return block.id;
}

}  // namespace

const BlockCatalogService::CachedCatalog& BlockCatalogService::cached_catalog() const {
    if (!cached_catalog_.has_value()) {
        auto blocks = provider_.list();
        std::sort(blocks.begin(), blocks.end(), [](const domain::BlockDescriptor& left, const domain::BlockDescriptor& right) {
            if (left.category != right.category) {
                return left.category < right.category;
            }
            if (left.name != right.name) {
                return left.name < right.name;
            }
            return left.id < right.id;
        });
        CachedCatalog catalog;
        std::unordered_set<std::string> browse_keys;
        for (const auto& block : blocks) {
            catalog.exact_blocks.insert_or_assign(block.id, block);
            const auto key = canonical_catalog_key(block);
            if (browse_keys.insert(key).second) {
                catalog.browse_blocks.push_back(block);
            }
        }
        cached_catalog_ = std::move(catalog);
    }
    return *cached_catalog_;
}

}  // namespace gr4cp::app
