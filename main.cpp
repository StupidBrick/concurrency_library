#include "executors/thread_pool/with_waitidle/thread_pool.hpp"
#include "fibers/api.hpp"
#include "channels/select.hpp"
#include "executors/manual_executor.hpp"

int main() {
    Executors::WithWaitIdle::ThreadPool tp{ 4 };

    Channels::Channel<int> channel1(1);
    Channels::Channel<double> channel2(1);

    Fibers::Go(tp, [&channel1]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        channel1.Send(1);
    });

    Fibers::Go(tp, [&channel2]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        channel2.Send(2);
    });

    Fibers::Go(tp, [&channel1, &channel2]() mutable {
        for (size_t i = 0; i < 2; ++i) {
            std::cout << "start of select\n";
            auto result = Channels::Select(channel1, channel2);
            if (std::holds_alternative<std::monostate>(result)) {
                std::cout << "No value\n";
            }
            if (std::holds_alternative<int>(result)) {
                std::cout << "int : " << std::get<int>(result) << '\n';
            }
            if (std::holds_alternative<double>(result)) {
                std::cout << "double : " << std::get<double>(result) << '\n';
            }
        }
    });

    tp.WaitIdle();
    tp.Stop();
}