#ifndef MAIN_H
#define MAIN_H

#include <vector>
#include <optional>

#include <mutex>

using data_t = std::vector<int>;

struct median_mutex_t {
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<int> median;
    
    void wait() {
        std::unique_lock<std::mutex> lck(mutex);
        while (!median) cv.wait(lck);
    }
};
using median_mutexes_t = std::vector<median_mutex_t>;

std::future<std::vector<int>> do_math(data_t & data, size_t cpu_num, size_t ncpu, median_mutexes_t & median_mutexes);


//Service code

struct params_t {
    size_t ncpu;
    std::optional<std::string> data_file_name;
    bool do_comparison;
};

params_t get_params(int paramc, char * const * paramv);
std::vector<int> read_array(params_t const & params);
void abort_usage(char const * error = nullptr);

#endif /* MAIN_H */

