#include <cstdint>
#include <cstddef>
#include <cstring>
extern "C" {
#ifdef HAVE_VERUS
void verus_hash(void* result, const void* data, const uint64_t len);
#endif
}
void verus_hash_2_2(const uint8_t* data, size_t len, uint8_t out[32]) {
#ifdef HAVE_VERUS
    verus_hash(out, data, (uint64_t)len);
#else
    std::memset(out, 0, 32); // stub if fetch failed (not valid for mining)
#endif
}
