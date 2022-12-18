#pragma once

#include <variant>
#include "channel.hpp"
#include <random>
#include <algorithm>
#include <array>

namespace Channels {

    namespace Detail {

        namespace TypeTraits {
            // return std::true_type if Args not contain X
            template<typename X, typename ...Args>
            struct IsUnique {
                using Flag = std::true_type;
            };

            template<typename X, typename ...Args>
            struct IsUnique<X, X, Args...> {
                using Flag = std::false_type;
            };

            template<typename X, typename Y, typename ...Args>
            struct IsUnique<X, Y, Args...> {
                using Flag = typename IsUnique<X, Args...>::Flag;
            };

            template<typename ...UniqueArgs>
            struct GetSelectorValue {
                template<typename X, typename ...Args>
                struct SelectorValue {
                    using Type = typename std::conditional_t<std::is_same_v<typename IsUnique<X, Args...>::
                    Flag, std::true_type>,
                            typename GetSelectorValue<UniqueArgs..., X>::template SelectorValue<Args...>::Type,
                            typename GetSelectorValue<UniqueArgs...>::template SelectorValue<Args...>::Type>;
                };

                template<typename X>
                struct SelectorValue<X> {
                    using Type = std::variant<UniqueArgs..., X>;
                };
            };

            template <typename ...Args>
            struct GetArgsCount {
                static constexpr size_t kArgsCount = 0;
            };

            template <>
            struct GetArgsCount<> {
                static constexpr size_t kArgsCount = 0;
            };

            template <typename X, typename ...Args>
            struct GetArgsCount<X, Args...> {
                static constexpr size_t kArgsCount = GetArgsCount<Args...>::kArgsCount + 1;
            };

            template <typename ...Args>
            struct MultiTypeArray {
            };

            template <typename T>
            struct MultiTypeArray<T> {
                virtual ~MultiTypeArray() = default;

                explicit MultiTypeArray(T&& value) : value_(std::forward<T>(value)) {
                }

                template <typename TEmplace>
                explicit MultiTypeArray(TEmplace emplace) : value_(emplace) {
                }

                T value_;
            };

            template <typename T, typename U, typename ...Args>
            struct MultiTypeArray<T, U, Args...> : public MultiTypeArray<U, Args...> {
            public:
                MultiTypeArray(T&& first_value, U&& second_value, Args&&... other_args) :
                        MultiTypeArray<U, Args...>(std::forward<U>(second_value), std::forward<Args>(other_args)...),
                        value_(std::forward<T>(first_value)) {
                }

                template <typename TEmplace, typename UEmplace, typename ...ArgsEmplace>
                MultiTypeArray(TEmplace first_value, UEmplace second_value, ArgsEmplace... other_args) :
                        MultiTypeArray<U, Args...>(second_value, other_args...), value_(first_value) {
                }

                T value_;

                virtual ~MultiTypeArray() = default;
            };

            template <size_t Index, typename ...Args>
            struct GetValueByIndex {
            };

            template <typename T, typename ...Args>
            struct GetValueByIndex<0, T, Args...> {
                using ReturnType = T;

                static const ReturnType& Get(const MultiTypeArray<T, Args...>& array) {
                    return array.value_;
                }

                static ReturnType& Get(MultiTypeArray<T, Args...>& array) {
                    return array.value_;
                }

                static const ReturnType& GetConst(MultiTypeArray<T, Args...>* array) {
                    return array->value_;
                }

                static ReturnType& Get(MultiTypeArray<T, Args...>* array) {
                    return array->value_;
                }
            };

            template <size_t Index, typename T, typename ...Args>
            struct GetValueByIndex<Index, T, Args...> {
                using ReturnType = typename GetValueByIndex<Index - 1, Args...>::ReturnType;

                static const ReturnType& Get(const MultiTypeArray<T, Args...>& array) {
                    return GetConst(&array);
                }

                static ReturnType& Get(MultiTypeArray<T, Args...>& array) {
                    return Get(&array);
                }

                static const ReturnType& GetConst(MultiTypeArray<T, Args...>* array) {
                    auto* new_array = (MultiTypeArray<Args...>*)array;
                    return GetValueByIndex<Index - 1, Args...>::GetConst(new_array);
                }

                static ReturnType& Get(MultiTypeArray<T, Args...>* array) {
                    auto* new_array = (MultiTypeArray<Args...>*)array;
                    return GetValueByIndex<Index - 1, Args...>::Get(new_array);
                }
            };

            template <size_t Index, typename ...Args>
            auto& Get(MultiTypeArray<Args...>& array) {
                return GetValueByIndex<Index, Args...>::Get(array);
            }

            template <size_t Index, typename ...Args>
            const auto& Get(const MultiTypeArray<Args...>& array) {
                return GetValueByIndex<Index, Args...>::Get(array);
            }

        }


        thread_local std::random_device device;
        thread_local std::mt19937 generator(device());


        namespace ForSelector {
            template <size_t Ind, size_t ArraySize, typename MaybeSelectorValue, typename X, typename ...Args>
            struct GetArray {
                static void Get(TypeTraits::MultiTypeArray<ChannelForSelect<X, MaybeSelectorValue, ArraySize>,
                        ChannelForSelect<Args, MaybeSelectorValue, ArraySize>...>& arr,
                                std::array<IChannelForSelect<MaybeSelectorValue, ArraySize>*, ArraySize>& result) {
                    IChannelForSelect<MaybeSelectorValue, ArraySize>& c = TypeTraits::Get<Ind>(arr);
                    result[Ind] = &c;
                    GetArray<Ind + 1, ArraySize, MaybeSelectorValue, X, Args...>::Get(arr, result);
                }
            };

            template <size_t Ind, typename MaybeSelectorValue, typename X, typename ...Args>
            struct GetArray<Ind, Ind, MaybeSelectorValue, X, Args...> {
                static void Get(TypeTraits::MultiTypeArray<ChannelForSelect<X, MaybeSelectorValue, Ind>,
                        ChannelForSelect<Args, MaybeSelectorValue, Ind>...>&,
                                std::array<IChannelForSelect<MaybeSelectorValue, Ind>*, Ind>&) {
                }
            };

            template <size_t Ind, size_t ArraySize, typename MaybeSelectorValue, typename X, typename ...Args>
            struct GetAwaiters {
                static void Get(TypeTraits::MultiTypeArray<Fibers::Awaiters::SelectorAwaiter<X, MaybeSelectorValue,
                                ArraySize>, Fibers::Awaiters::SelectorAwaiter<Args, MaybeSelectorValue,
                                ArraySize>...>& arr, std::array<std::pair<Intrusive::BidirectionalListNode*,
                                Intrusive::List*>, ArraySize>& awaiters) {
                    auto& awaiter = TypeTraits::Get<Ind>(arr);
                    awaiter.SetIndex(Ind);
                    awaiters[Ind] = { (Intrusive::BidirectionalListNode*)&awaiter, nullptr };
                    GetAwaiters<Ind + 1, ArraySize, MaybeSelectorValue, X, Args...>::Get(arr, awaiters);
                }
            };

            template <size_t Ind, typename MaybeSelectorValue, typename X, typename ...Args>
            struct GetAwaiters<Ind, Ind, MaybeSelectorValue, X, Args...> {
                static void Get(TypeTraits::MultiTypeArray<Fibers::Awaiters::SelectorAwaiter<X, MaybeSelectorValue,
                        Ind>, Fibers::Awaiters::SelectorAwaiter<Args, MaybeSelectorValue,
                        Ind>...>&, std::array<std::pair<Intrusive::BidirectionalListNode*,
                        Intrusive::List*>, Ind>&) {
                }
            };
        }


        template<typename X, typename ...Args>
        class Selector {
        private:
            using SelectorValue = typename Detail::TypeTraits::GetSelectorValue<>::SelectorValue<X, Args...>::Type;
            using MaybeSelectorValue = typename Detail::TypeTraits::GetSelectorValue<>::
            SelectorValue<X, std::monostate, Args...>::Type;

            static constexpr size_t kArgsCount = TypeTraits::GetArgsCount<X, Args...>::kArgsCount;

        public:
            static MaybeSelectorValue Select(Channel<X>& xs, Channel<Args>&... args) {
                TypeTraits::MultiTypeArray<ChannelForSelect<X, MaybeSelectorValue, kArgsCount>,
                        ChannelForSelect<Args,
                                MaybeSelectorValue, kArgsCount>...> arr(GetChannelForSelect<X, MaybeSelectorValue, kArgsCount>(xs),
                                                            GetChannelForSelect<Args, MaybeSelectorValue, kArgsCount>(args)...);
                std::array<IChannelForSelect<MaybeSelectorValue, kArgsCount>*, kArgsCount> channels =
                        GetChannelArray(arr);

                MaybeSelectorValue result = std::monostate();
                std::atomic_flag is_result_set{ false };

                std::array<std::pair<Intrusive::BidirectionalListNode*, Intrusive::List*>, kArgsCount> awaiters;

                TypeTraits::MultiTypeArray<Fibers::Awaiters::SelectorAwaiter<X, MaybeSelectorValue, kArgsCount>,
                        Fibers::Awaiters::SelectorAwaiter<Args, MaybeSelectorValue, kArgsCount>...> awaiters_arr(
                                Fibers::Awaiters::SelectorAwaiter<X, MaybeSelectorValue, kArgsCount>(
                                        Fibers::Fiber::Self(), result, awaiters, is_result_set
                                        ),
                                Fibers::Awaiters::SelectorAwaiter<Args, MaybeSelectorValue, kArgsCount>(
                                        Fibers::Fiber::Self(), result, awaiters, is_result_set
                                        )...
                                );

                ForSelector::GetAwaiters<0, kArgsCount, MaybeSelectorValue, X, Args...>::Get(awaiters_arr, awaiters);

                GenerateRandomPermutation(channels, awaiters);

                for (size_t i = 0; i < kArgsCount; ++i) {
                    if (channels[i]->Receive((Fibers::Awaiters::ChannelConsumerAwaiterBase*)(awaiters[i].first))) {
                        return std::move(result);
                    }
                }

                Fibers::Self::Suspend((Fibers::Awaiters::ChannelConsumerAwaiterBase*)awaiters[0].first);
                return std::move(result);
            }

            static MaybeSelectorValue TrySelect(Channel<X>& xs, Channel<Args>&... args) {
                TypeTraits::MultiTypeArray<ChannelForSelect<X, MaybeSelectorValue, kArgsCount>,
                        ChannelForSelect<Args,
                        MaybeSelectorValue, kArgsCount>...> arr(GetChannelForSelect<X, MaybeSelectorValue, kArgsCount>(xs),
                                                    GetChannelForSelect<Args, MaybeSelectorValue, kArgsCount>(args)...);
                auto channels = GetChannelArray(arr);
                GenerateRandomPermutation(channels);

                MaybeSelectorValue result = std::monostate();

                for (size_t i = 0; i < kArgsCount; ++i) {
                    if (channels[i]->TryReceive(result)) {
                        return std::move(result);
                    }
                }

                return std::move(result);
            }

        private:
            static void GenerateRandomPermutation(std::array<IChannelForSelect<MaybeSelectorValue, kArgsCount>*,
                    kArgsCount>& arr) {
                std::shuffle(arr.begin(), arr.end(), generator);
            }


            static void GenerateRandomPermutation(std::array<IChannelForSelect<MaybeSelectorValue, kArgsCount>*,
                    kArgsCount>& channels, std::array<std::pair<Intrusive::BidirectionalListNode*, Intrusive::List*>,
                            kArgsCount>& awaiters) {
                std::uniform_int_distribution<int> D;
                for (int i = kArgsCount - 1; i > 0; --i) {
                    int index = D(generator, std::uniform_int_distribution<int>::param_type(0, i));
                    std::swap(channels[i], channels[index]);
                    std::swap(awaiters[i].first, awaiters[index].first);

                    ((Fibers::Awaiters::ChannelConsumerAwaiterBase*)awaiters[i].first)->SetIndex(i);
                }
            }


            static auto GetChannelArray(TypeTraits::MultiTypeArray<ChannelForSelect<X, MaybeSelectorValue, kArgsCount>,
                    ChannelForSelect<Args, MaybeSelectorValue, kArgsCount>...>& arr) {
                std::array<IChannelForSelect<MaybeSelectorValue, kArgsCount>*, kArgsCount> result;
                ForSelector::GetArray</*Ind=*/0, /*ArraySize=*/kArgsCount, MaybeSelectorValue, X, Args...>::Get(arr,
                                                                                                                result);
                return std::move(result);
            }
        };

    }


    template <typename X, typename ...Args>
    auto Select(Channel<X>& xs, Channel<Args>&... args) {
        return Detail::Selector<X, Args...>::Select(xs, args...);
    }

    template <typename X, typename ...Args>
    auto TrySelect(Channel<X>& xs, Channel<Args>&... args) {
        return Detail::Selector<X, Args...>::TrySelect(xs, args...);
    }

}