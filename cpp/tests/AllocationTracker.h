#pragma once
// One translation unit per test executable. Tracks replaceable C++ allocation APIs,
// including aligned and nothrow forms. Does not intercept direct malloc or OS calls.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
namespace allocation_tracker
{
inline std::atomic<std::size_t> liveBytes{0}, liveBlocks{0};
inline thread_local bool processing = false;
inline std::atomic<std::size_t> processAllocations{0}, processFrees{0};
struct Header { void* base; std::size_t bytes; };
inline void* allocate(std::size_t bytes, std::size_t alignment)
{
    alignment = alignment < alignof(std::max_align_t) ? alignof(std::max_align_t) : alignment;
    if (bytes > std::numeric_limits<std::size_t>::max() - alignment - sizeof(Header)) throw std::bad_alloc();
    auto* base = std::malloc(bytes + alignment + sizeof(Header));
    if (!base) throw std::bad_alloc();
    const auto address = (reinterpret_cast<std::uintptr_t>(base) + sizeof(Header) + alignment - 1) & ~(alignment - 1);
    auto* header = reinterpret_cast<Header*>(address) - 1;
    header->base = base; header->bytes = bytes;
    liveBytes.fetch_add(bytes); liveBlocks.fetch_add(1);
    if (processing) processAllocations.fetch_add(1);
    return reinterpret_cast<void*>(address);
}
inline void release(void* pointer) noexcept
{
    if (!pointer) return;
    auto* header = reinterpret_cast<Header*>(pointer) - 1;
    liveBytes.fetch_sub(header->bytes); liveBlocks.fetch_sub(1);
    if (processing) processFrees.fetch_add(1);
    std::free(header->base);
}
struct ProcessScope
{
    ProcessScope() { processing = true; }
    ~ProcessScope() { processing = false; }
};
}
// Sanitizer runs use the native allocator so ASan owns precise allocation redzones.
#ifndef MULTI_EFFECTS_TEST_SANITIZER
void* operator new(std::size_t n) { return allocation_tracker::allocate(n, alignof(std::max_align_t)); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { allocation_tracker::release(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
void* operator new(std::size_t n, std::align_val_t a) { return allocation_tracker::allocate(n, std::size_t(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { ::operator delete(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept { return ::operator new(n, t); }
void operator delete(void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new(n, a); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept { return ::operator new(n, a, t); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { ::operator delete(p); }

#endif
