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

    void set_median(int median_) {
        std::unique_lock<std::mutex> lck(mutex);
        median = median_;
        cv.notify_all();
    }
};
using median_mutexes_t = std::vector<median_mutex_t>;

data_t do_math_mt(data_t & data, size_t ncpu);
std::future<data_t> do_math_mt_(data_t & data, size_t cpu_num, size_t ncpu, median_mutexes_t & median_mutexes);

data_t do_math_st(data_t & data, size_t ncpu);

//Service code

struct params_t {
    size_t ncpu;
    std::optional<std::string> data_file_name;
    bool do_comparison;
    bool single_threaded;
};

params_t get_params(int paramc, char * const * paramv);
std::vector<int> read_array(params_t const & params);
void abort_usage(char const * error = nullptr);

#endif /* MAIN_H */

