#pragma once

#include <optional>
#include <atomic>
#include <memory>
#include <array>

namespace Channels {

    namespace Detail {
        template <typename T>
        class ChannelImpl {
            explicit ChannelImpl(size_t capacity) {
                // TODO
            }

            void Send(T&& value) {
                // TODO
            }

            bool TrySend(T&& value) {
                // TODO
            }

            T Receive() {
                // TODO
            }

            std::optional<T> TryReceive() {
                // TODO
            }

        private:

        };
    }


    template <typename T>
    class Channel {
    private:
        using Impl = Detail::ChannelImpl<T>;

    public:
        explicit Channel(size_t capacity) : impl_(std::make_shared<Impl>(capacity)) {
        }

        void Send(T&& value) {
            impl_->Send(std::forward<T>(value));
        }

        bool TrySend(T&& value) {
            impl_->TrySend(std::forward<T>(value));
        }

        T Receive() {
            return std::move(impl_->Receive());
        }

        std::optional<T> TryReceive() {
            return std::move(impl_->TryReceive());
        }

    private:
        std::shared_ptr<Impl> impl_;
    };

}