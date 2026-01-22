export module Render.CommandListSubmissionContext;

import Core.Prelude;
import Core.Utilities;
import Vendor.ApplicationAPI;

namespace
Engine {
    export class CommandListSubmissionContext : public RefCounted {
    public:
        CommandListSubmissionContext(nvrhi::DeviceHandle device) : mNvrhiDevice(std::move(device)) {
            mWorkerThread = std::thread([this]() {
                while (true) {
                    std::move_only_function<void()> task;

                    // scope
                    {
                        std::unique_lock<std::mutex> lock(mTaskQueueMutex);
                        mTaskQueueCondition.wait(lock, [this]() {
                            return !mTaskQueue.empty() || mStopThread.load();
                        });

                        if (mStopThread.load() && mTaskQueue.empty()) {
                            return;
                        }

                        task = std::move(mTaskQueue.front());
                        mTaskQueue.pop();
                    }

                    std::invoke(std::move(task));
                }
            });
        }

        ~CommandListSubmissionContext() override {
            Stop();
            if (mWorkerThread.joinable()) {
                mWorkerThread.join();
            }
        }

        void SubmitTaskImmediate(std::move_only_function<void()> task) {
            SubmitTaskAsync(std::move(task)).get();
        }

        std::future<void> SubmitTaskAsync(std::move_only_function<void()> task) {
            auto taskPromise = std::make_shared<std::promise<void>>();
            std::future<void> taskFuture = taskPromise->get_future(); {
                std::lock_guard<std::mutex> lock(mTaskQueueMutex);
                mTaskQueue.push([task = std::move(task), promise = std::move(taskPromise)]() mutable {
                    try {
                        task();
                        promise->set_value();
                    } catch (...) {
                        promise->set_exception(std::current_exception());
                    }
                });
            }
            mTaskQueueCondition.notify_one();
            return taskFuture;
        }

        void SubmitTaskImmediate(std::move_only_function<void(CommandListSubmissionContext &ctx)> task) {
            SubmitTaskAsync(std::move(task)).get();
        }

        std::future<void> SubmitTaskAsync(std::move_only_function<void(CommandListSubmissionContext &ctx)> task) {
            return SubmitTaskAsync([task = std::move(task), self = RefFromThis()]() mutable {
                task(*self);
            });
        }

        void Stop() {
            mStopThread.store(true);
            mTaskQueueCondition.notify_all();
        }

        const nvrhi::DeviceHandle &GetDevice() const {
            return mNvrhiDevice;
        }

        std::function<void(std::move_only_function<void()>)> CreateSubmitter() {
            return [self = RefFromThis()](std::move_only_function<void()> task) {
                self->SubmitTaskImmediate([task = std::move(task)] mutable {
                    task();
                });
            };
        }

    private:
        std::mutex mTaskQueueMutex;
        std::condition_variable mTaskQueueCondition;
        std::queue<std::move_only_function<void()>> mTaskQueue;
        std::atomic<bool> mStopThread{false};
        std::thread mWorkerThread;

    private:
        nvrhi::DeviceHandle mNvrhiDevice;
    };
}
