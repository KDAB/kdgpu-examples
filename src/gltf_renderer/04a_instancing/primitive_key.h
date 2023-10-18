#pragma once

#include <KDGpu/utils/hash_utils.h>

#include <stdint.h>

struct PrimitiveKey {
    uint32_t meshIndex{ 0 };
    uint32_t primitiveIndex{ 0 };

    bool operator==(const PrimitiveKey &other) const noexcept
    {
        return meshIndex == other.meshIndex && primitiveIndex == other.primitiveIndex;
    }
};

namespace std {

template<>
struct hash<PrimitiveKey> {
    size_t operator()(const PrimitiveKey &key) const
    {
        uint64_t hash = 0;
        KDGpu::hash_combine(hash, key.meshIndex);
        KDGpu::hash_combine(hash, key.primitiveIndex);
        return hash;
    }
};

} // namespace std
