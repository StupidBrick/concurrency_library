#pragma once

#include <optional>
#include <atomic>
#include <memory>
#include "../detail/spinlock.hpp"
#include "../fibers/awaiters.hpp"
#include "../fibers/api.hpp"
#include "../intrusive/structures/queue.hpp"
#include "../intrusive/structures/list.hpp"

namespace Channels {

    template <typename T>
    class Channel;

    namespace Detail {
        
        template <typename T>
        class ChannelImpl {
        private:
            template <typename X, typename Variant, size_t AwaitersCount>
            friend class ChannelForSelect;

        public:
            explicit ChannelImpl(size_t capacity) : kCapacity(capacity), buffer_(new T[capacity]) {
            }

            void Send(T&& value) {
                ::Detail::QueueSpinLock::Guard guard(spinlock_);
                if (TrySendImpl(std::forward<T>(value))) {
                    return;
                }

                Fibers::Awaiters::ChannelProducerAwaiter<T> awaiter(Fibers::Fiber::Self(),
                                                                    std::forward<T>(value), guard);
                producers_queue_.Push(&awaiter);
                Fibers::Self::Suspend(&awaiter);
            }

            bool TrySend(T&& value) {
                ::Detail::QueueSpinLock::Guard guard(spinlock_);
                return TrySendImpl(std::forward<T>(value));
            }

            T Receive() {
                ::Detail::QueueSpinLock::Guard guard(spinlock_);
                if (tail_ - head_ > 0) {
                    T result =  std::move(buffer_[(head_++) % kCapacity]);
                    TryTakeValueFromProducersQueue();
                    return std::move(result);
                }

                std::optional<T> result;
                Fibers::Awaiters::ChannelConsumerAwaiter<T> awaiter(Fibers::Fiber::Self(),
                                                                    result, guard);
                consumers_queue_.PushBack(&awaiter);
                Fibers::Self::Suspend(&awaiter);
                return std::move(*result);
            }

            std::optional<T> TryReceive() {
                ::Detail::QueueSpinLock::Guard guard(spinlock_);
                if (tail_ - head_ > 0) {
                    std::optional<T> result =  { std::move(buffer_[(head_++) % kCapacity]) };
                    TryTakeValueFromProducersQueue();
                    return std::move(result);
                }

                return std::nullopt;
            }

        private:
            bool TrySendImpl(T&& value) {
                if (consumers_queue_.Size() > 0) {
                    auto* awaiter = (Fibers::Awaiters::IChannelConsumerAwaiter<T>*)consumers_queue_.TryPopFront();
                    awaiter->Resume(std::forward<T>(value));
                    return true;
                }

                if (tail_ - head_ < kCapacity) {
                    buffer_[(tail_++) % kCapacity] = std::forward<T>(value);
                    return true;
                }

                return false;
            }

            void TryTakeValueFromProducersQueue() {
                if (producers_queue_.Size() > 0) {
                    auto* awaiter = (Fibers::Awaiters::ChannelProducerAwaiter<T>*)producers_queue_.TryPop();
                    buffer_[(tail_++) % kCapacity] = std::move(awaiter->Resume());
                }
            }

            bool SelectorReceive(Fibers::Awaiters::ChannelConsumerAwaiterBase* awaiter) {
                ::Detail::QueueSpinLock::Guard guard(spinlock_);
                auto* result_awaiter = (Fibers::Awaiters::IChannelConsumerAwaiter<T>*)awaiter;

                if (tail_ - head_ > 0) {
                    result_awaiter->Resume(std::move(buffer_[(head_++) % kCapacity]));
                    TryTakeValueFromProducersQueue();
                    return true;
                }

                consumers_queue_.PushBack(awaiter);
                result_awaiter->SetQueue(&consumers_queue_);
                return false;
            }

        private:
            const size_t kCapacity;
            std::unique_ptr<T[]> buffer_;

            size_t head_ = 0;
            size_t tail_ = 0;

            ::Detail::QueueSpinLock spinlock_;

            Intrusive::List consumers_queue_;
            Intrusive::Queue producers_queue_;
        };



        template <typename Variant, size_t AwaitersCount>
        class IChannelForSelect {
        public:
            virtual ~IChannelForSelect() = default;

            virtual bool Receive(Fibers::Awaiters::ChannelConsumerAwaiterBase*) = 0;

            virtual bool TryReceive(Variant& result) = 0;
        };

        template <typename T, typename Variant, size_t AwaitersCount>
        class ChannelForSelect : public IChannelForSelect<Variant, AwaitersCount> {
        public:
            explicit ChannelForSelect(ChannelImpl<T>& impl) : impl_(&impl) {
            }

            // return true if channel have value
            bool Receive(Fibers::Awaiters::ChannelConsumerAwaiterBase* awaiter) override {
                return impl_->SelectorReceive(awaiter);
            }

            bool TryReceive(Variant& result) override {
                std::optional<T> local_result = impl_->TryReceive();
                if (local_result == std::nullopt) {
                    return false;
                }
                result = std::move(*local_result);
                return true;
            }

        private:
            ChannelImpl<T>* impl_ = nullptr;
        };

        template <typename T, typename Variant, size_t AwaitersCount>
        ChannelForSelect<T, Variant, AwaitersCount> GetChannelForSelect(const Channel<T>& channel);
    }


    template <typename T>
    class Channel {
    private:
        using Impl = Detail::ChannelImpl<T>;

        template <typename U, typename Variant, size_t AwaitersCount>
        friend Detail::ChannelForSelect<U, Variant, AwaitersCount> Detail::GetChannelForSelect(const Channel<U>& channel);

    public:
        explicit Channel(size_t capacity) : impl_(std::make_shared<Impl>(capacity)) {
        }

        void Send(T&& value) {
            impl_->Send(std::forward<T>(value));
        }

        bool TrySend(T&& value) {
            return impl_->TrySend(std::forward<T>(value));
        }

        T Receive() {
            return std::move(impl_->Receive());
        }

        std::optional<T> TryReceive() {
            return std::move(impl_->TryReceive());
        }

    private:
        template <typename Variant, size_t AwaitersCount>
        Detail::ChannelForSelect<T, Variant, AwaitersCount> GetChannelForSelect() const {
            return Detail::ChannelForSelect<T, Variant, AwaitersCount>(*impl_);
        }

    private:
        std::shared_ptr<Impl> impl_;
    };

    namespace Detail {
        template <typename T, typename Variant, size_t AwaitersCount>
        ChannelForSelect<T, Variant, AwaitersCount> GetChannelForSelect(const Channel<T>& channel) {
            return channel.template GetChannelForSelect<Variant, AwaitersCount>();
        }
    }

}