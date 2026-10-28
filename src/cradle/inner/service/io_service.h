#ifndef CRADLE_INNER_SERVICE_IO_SERVICE_H
#define CRADLE_INNER_SERVICE_IO_SERVICE_H

#ifdef __APPLE__

#include <chrono>
#include <cstddef>
#include <coroutine>
#include <memory>
#include <optional>
#include <utility>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cppcoro/cancellation_registration.hpp>
#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/operation_cancelled.hpp>

namespace cradle {

class io_service
{
 public:
    class timed_operation
    {
        struct state
        {
            state(boost::asio::io_context& context,
                  cppcoro::cancellation_token token)
                : timer(context), token(std::move(token))
            {
            }

            boost::asio::steady_timer timer;
            cppcoro::cancellation_token token;
            std::optional<cppcoro::cancellation_registration> registration;
        };

     public:
        timed_operation(boost::asio::io_context& context,
                        std::chrono::steady_clock::duration delay,
                        cppcoro::cancellation_token token)
            : context_(context), delay_(delay),
              state_(std::make_shared<state>(context, std::move(token)))
        {
        }

        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> continuation)
        {
            auto shared = state_;
            shared->registration.emplace(shared->token,
                [weak = std::weak_ptr<state>(shared), &context = context_]() {
                    boost::asio::post(context, [weak]() {
                        if (auto pending = weak.lock())
                        {
                            pending->timer.cancel();
                        }
                    });
                });
            boost::asio::post(context_, [shared, delay = delay_, continuation]() {
                if (shared->token.is_cancellation_requested())
                {
                    continuation.resume();
                    return;
                }
                shared->timer.expires_after(delay);
                shared->timer.async_wait(
                    [shared, continuation](boost::system::error_code) {
                        continuation.resume();
                    });
            });
        }

        void await_resume()
        {
            state_->registration.reset();
            if (state_->token.is_cancellation_requested())
            {
                throw cppcoro::operation_cancelled{};
            }
        }

     private:
        boost::asio::io_context& context_;
        std::chrono::steady_clock::duration delay_;
        std::shared_ptr<state> state_;
    };

    template<typename Rep, typename Period>
    timed_operation schedule_after(
        std::chrono::duration<Rep, Period> delay,
        cppcoro::cancellation_token token = {})
    {
        return timed_operation(context_,
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay),
            std::move(token));
    }

    std::size_t process_events()
    {
        return context_.run();
    }

    void stop() noexcept
    {
        context_.stop();
    }

 private:
    boost::asio::io_context context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_{boost::asio::make_work_guard(context_)};
};

} // namespace cradle

#else

#include <cppcoro/io_service.hpp>

namespace cradle {
using io_service = cppcoro::io_service;
}

#endif

#endif