#pragma once
#include <atomic>

namespace LockFree {

    namespace Detail {
        template <typename T>
        struct ICountedPtr {
            // TODO
        };

        // bool + 15bit unsigned integer + 48bit pointer
        template <typename T>
        struct CountedPtr : public ICountedPtr<T> {
        private:
            static const int kPointerSize = 48;
            static const int kPointerAnsCounterSize = 63;
            static const uint64_t kPointerMask = 0x0000ffffffffffff;
            static const uint64_t kCounterMask = 0x7fff000000000000;
            static const uint64_t kFlagMask = ((uint64_t)1 << kPointerAnsCounterSize);

            uint64_t dirty_ptr_ = 0;

        public:
            // TODO
        };

        // bool + 15bit unsigned integer + 48bit pointer
        template <typename T>
        struct AtomicCountedPtr : public ICountedPtr<T> {
        private:
            static const int kPointerSize = 48;
            static const int kPointerAnsCounterSize = 63;
            static const uint64_t kPointerMask = 0x0000ffffffffffff;
            static const uint64_t kCounterMask = 0x7fff000000000000;
            static const uint64_t kFlagMask = ((uint64_t)1 << kPointerAnsCounterSize);

            std::atomic<uint64_t> dirty_ptr_ = 0;

        public:
            // TODO
        };


        struct ControlBlock {
            static const long long kOneCounter = ((long long)1 << 32);

            void* value;
            std::atomic<long long> internal_count;

            ControlBlock(void* value,
                         long long internal_count,
                         long long atomic_ptr_count) : value(value), internal_count(
                                 internal_count + (atomic_ptr_count << 32)
                                 ) {
            }
        };
    }


    template <typename T>
    class SharedPtr {
    public:
        SharedPtr() = default;

        SharedPtr(T* ptr);

        SharedPtr(const SharedPtr& other);

        SharedPtr(SharedPtr&& other) noexcept;

        SharedPtr& operator=(const SharedPtr& other);

        SharedPtr& operator=(SharedPtr&& other) noexcept;

        T* Get();

        const T* Get() const;

        void Release();

        ~SharedPtr() noexcept;

    private:
        Detail::ICountedPtr<Detail::ControlBlock>* control_block_ptr_ = nullptr;
    };


    template <typename T>
    class AtomicSharedPtr {
    public:
        // TODO
    };


    template <typename T>
    SharedPtr<T>::SharedPtr(T *ptr) {

    }

}