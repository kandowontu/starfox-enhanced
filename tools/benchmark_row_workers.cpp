#include "starfox/render/row_workers.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

static std::atomic<std::size_t> allocations{};
void* operator new(std::size_t count) {
    if (void* result=std::malloc(count ? count : 1)) {
        ++allocations;
        return result;
    }
    throw std::bad_alloc{};
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value,std::size_t) noexcept { std::free(value); }

int main() {
    starfox::render::RowWorkers workers;
    workers.set_worker_count(4);
    std::vector<unsigned> output(224);
    const std::function<void(std::uint32_t,std::uint32_t)> body=
        [&](auto first,auto last) { for (auto y=first;y<last;++y) ++output[y]; };
    for (unsigned i=0;i<100;++i) workers.parallel_rows(224,body);
    std::vector<double> times(1000);
    allocations=0;
    for (auto& time:times) {
        const auto start=std::chrono::steady_clock::now();
        workers.parallel_rows(224,body);
        time=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
    }
    const auto count=allocations.load();
    if (!std::all_of(output.begin(),output.end(),[](auto n) { return n==1100; })) return 1;
    std::sort(times.begin(),times.end());
    std::cout << "allocations=" << count << " median_us=" << times[500]
        << " p95_us=" << times[950] << '\n';
}
