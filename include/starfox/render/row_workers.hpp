#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace starfox::render {

// A persistent pool for the presentation passes, which are all row
// independent: each one reads either the indexed framebuffer, a snapshot taken
// before the pass, or a parallel surface buffer, and writes one output pixel
// per input pixel. Render Upscale multiplies their cost by the square of the
// scale, so at 6x and above they dominate the frame while seven cores idle.
//
// The pool is created once and reused, because at a high render FPS the frame
// budget is a few milliseconds and spawning threads per frame would cost more
// than the work saved.
class RowWorkers {
public:
    RowWorkers() = default;
    ~RowWorkers();
    RowWorkers(const RowWorkers&) = delete;
    RowWorkers& operator=(const RowWorkers&) = delete;

    // 0 selects a default derived from hardware_concurrency. 1 disables the
    // pool and runs everything on the calling thread.
    void set_worker_count(std::size_t workers);
    [[nodiscard]] std::size_t worker_count() const noexcept { return workers_; }

    // Runs `body(first_row, last_row)` over a half-open partition of
    // [0, rows). The calling thread takes the first slice and waits for the
    // rest, so no work is queued behind an idle dispatcher. `body` must not
    // write outside the rows it is given.
    void parallel_rows(
        std::uint32_t rows,
        const std::function<void(std::uint32_t, std::uint32_t)>& body);

private:
    void stop_pool();

    std::vector<std::thread> pool_;
    std::mutex mutex_;
    std::condition_variable work_ready_;
    std::condition_variable work_done_;
    const std::function<void(std::uint32_t, std::uint32_t)>* job_{};
    std::vector<std::uint32_t> job_bounds_;
    std::size_t workers_{};
    std::size_t pending_{};
    std::uint64_t generation_{};
    bool stopping_{false};
};

} // namespace starfox::render
