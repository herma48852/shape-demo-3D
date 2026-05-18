#pragma once
#include <mutex>
#include <vector>
#include <utility>
#include <string>
#include "ShapeType.hpp" 

struct ReceivedSample {
    std::string topic_name;
    ShapeTypeExtended data;
};

class ThreadSafeSampleQueue {
private:
    std::mutex queue_mutex_;
    std::vector<ReceivedSample> sample_buffer_;

public:
    ThreadSafeSampleQueue() = default;
    ~ThreadSafeSampleQueue() = default;
    ThreadSafeSampleQueue(const ThreadSafeSampleQueue&) = delete;
    ThreadSafeSampleQueue& operator=(const ThreadSafeSampleQueue&) = delete;

    void push_sample(const std::string& topic_name, const ShapeTypeExtended& sample) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        sample_buffer_.push_back({topic_name, sample});
    }

    std::vector<ReceivedSample> drain_queue() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::vector<ReceivedSample> drained_samples = std::move(sample_buffer_);
        sample_buffer_.clear();
        return drained_samples;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        sample_buffer_.clear();
    }
};
