#include "starfox/render/row_workers.hpp"

#include <algorithm>

namespace starfox::render {
namespace {

// Below this a worker hand-off costs more than the rows it saves.
constexpr std::uint32_t minimum_rows_per_worker = 16U;

} // namespace

RowWorkers::~RowWorkers() { stop_pool(); }

void RowWorkers::stop_pool() {
    if (pool_.empty()) return;
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        stopping_ = true;
    }
    work_ready_.notify_all();
    for (auto& worker : pool_) {
        if (worker.joinable()) worker.join();
    }
    pool_.clear();
    generation_ = 0U;
    job_ = nullptr;
    job_bounds_.clear();
    pending_ = 0U;
    stopping_ = false;
}

void RowWorkers::set_worker_count(std::size_t workers) {
    if (workers == 0U) {
        const auto detected = std::thread::hardware_concurrency();
        workers = detected == 0U ? 1U : std::min<std::size_t>(detected, 8U);
    }
    if (workers == workers_ && workers == pool_.size() + 1U) return;
    stop_pool();
    workers_ = workers;
    if (workers_ <= 1U) return;
    pool_.reserve(workers_ - 1U);
    for (std::size_t index = 0; index < workers_ - 1U; ++index) {
        pool_.emplace_back([this, index] {
            std::uint64_t seen = 0U;
            for (;;) {
                const std::function<void(std::uint32_t, std::uint32_t)>* job{};
                std::uint32_t first = 0U;
                std::uint32_t last = 0U;
                {
                    std::unique_lock<std::mutex> lock{mutex_};
                    work_ready_.wait(lock, [this, &seen] {
                        return stopping_ || generation_ != seen;
                    });
                    if (stopping_) return;
                    seen = generation_;
                    // Slice 0 belongs to the dispatching thread.
                    const auto slot = (index + 1U) * 2U;
                    if (slot + 1U < job_bounds_.size()) {
                        first = job_bounds_[slot];
                        last = job_bounds_[slot + 1U];
                        job = job_;
                    }
                }
                if (job != nullptr && first < last) (*job)(first, last);
                {
                    const std::lock_guard<std::mutex> lock{mutex_};
                    if (--pending_ == 0U) work_done_.notify_one();
                }
            }
        });
    }
}

void RowWorkers::parallel_rows(
    std::uint32_t rows,
    const std::function<void(std::uint32_t, std::uint32_t)>& body) {
    if (rows == 0U) return;
    // A pool that was never configured would otherwise run every frame on the
    // calling thread. Choose the default on first use rather than trusting
    // each caller to remember, since one forgetting is invisible: the output
    // is identical, only slower.
    if (workers_ == 0U) set_worker_count(0U);
    const auto slices = pool_.empty()
        ? std::size_t{1U}
        : std::min<std::size_t>(pool_.size() + 1U,
            std::max<std::size_t>(1U, rows / minimum_rows_per_worker));
    if (slices <= 1U) {
        body(0U, rows);
        return;
    }

    const auto span = rows / static_cast<std::uint32_t>(slices);
    const auto remainder = rows % static_cast<std::uint32_t>(slices);

    {
        const std::lock_guard<std::mutex> lock{mutex_};
        // Reuse the persistent job storage instead of allocating a temporary
        // partition vector and copying it for every presentation pass.
        job_bounds_.resize(slices * 2U);
        std::uint32_t cursor = 0U;
        for (std::size_t slice = 0; slice < slices; ++slice) {
            const auto length = span
                + (slice < remainder ? std::uint32_t{1U} : std::uint32_t{0U});
            job_bounds_[slice * 2U] = cursor;
            cursor += length;
            job_bounds_[slice * 2U + 1U] = cursor;
        }
        job_ = &body;
        pending_ = pool_.size();
        ++generation_;
    }
    work_ready_.notify_all();
    body(0U, span + (remainder != 0U ? 1U : 0U));
    {
        std::unique_lock<std::mutex> lock{mutex_};
        work_done_.wait(lock, [this] { return pending_ == 0U; });
        job_ = nullptr;
        job_bounds_.clear();
    }
}

} // namespace starfox::render
