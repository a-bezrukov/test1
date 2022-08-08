#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>

#include <optional>

#include <thread>
#include <future>
#include <memory>

size_t get_ncpu(int paramc, char const * paramv[]);
std::vector<int> read_array();
void abort_usage(char const * error = nullptr);


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

int main (int paramc, char const * paramv[]) {
    size_t const ncpu = get_ncpu(paramc, paramv);
    
    std::cout << "Threads to be used: " << ncpu << std::endl;
    
    if(ncpu == 0) { // Measure the threshold
        
    } else {
        data_t data = read_array();
        
        if(data.size() < ncpu) {
            std::cerr << data.size() << std::endl;
            abort_usage("The number of input data elements is less than the number of threads.");
        }
        
        if(data.size() % ncpu) {
            abort_usage("The number of input data elements is not divisible by ncpu.");
        }
        
        median_mutexes_t median_mutexes (ncpu);
        
        std::vector<std::future<std::vector<int>>> result;
        result.reserve(ncpu);

        for(size_t i = 0; i < ncpu; ++i) {
            result.emplace_back(do_math(data, i, ncpu, median_mutexes));
        }
        
        for(size_t i = 0; i < ncpu; ++i) {
            for(int const & i: result[i].get()) {
                std::cout << i << " ";
            }
        }
        std::cout << std::endl;
    }
    return 0;
}


std::future<std::vector<int>> do_math(data_t & data, size_t cpu_num, size_t ncpu, median_mutexes_t & median_mutexes)
{
    return std::async([&data, cpu_num, ncpu, &median_mutexes]()
    {
        size_t const data_sz = data.size() / ncpu,
                median_num = data_sz / 2;
        
        data_t::iterator const ibeg = data.begin() + data_sz * cpu_num,
                imed = ibeg + median_num,
                iend = data.begin() + data_sz * (cpu_num + 1);
        
        std::nth_element(ibeg, imed, iend);
        
        median_mutexes[cpu_num].median = *(imed);
        median_mutexes[cpu_num].cv.notify_all();
        
        std::vector<int> result;
        
        if(cpu_num == 0) {
            median_mutexes[1].wait();
            int const upper_bound = *median_mutexes[1].median;
            std::copy_if(ibeg, iend, std::back_inserter(result), [upper_bound](int elem){return elem < upper_bound;});
        } else if(cpu_num == ncpu - 1) {
            median_mutexes[ncpu - 2].wait();
            int const lower_bound = *median_mutexes[ncpu - 2].median;
            std::copy_if(ibeg, iend, std::back_inserter(result), [lower_bound](int elem){return elem > lower_bound;});
        } else {
            median_mutexes[cpu_num - 1].wait();
            median_mutexes[cpu_num + 1].wait();
            int const lower_bound = *median_mutexes[cpu_num - 1].median;
            int const upper_bound = *median_mutexes[cpu_num + 1].median;
            std::copy_if(ibeg, iend, std::back_inserter(result), [lower_bound, upper_bound](int elem){return elem > lower_bound && elem < upper_bound;});
        }
        
        return result;
    });
}

// Service code

void abort_usage(char const * error) {
    if(error) {
        std::cerr << "Error: " << error << std::endl;
    } else {
        std::cerr 
                << "Usage: cat ints.file | test1 [NTHREADS]\n"
                << "  NTHREADS - unsigned - represents the number of threads to be used. If 0, the measuring activity will be done.\n"
                << "    If unspecified, the test uses one thread per the available CPU.\n"
                << std::endl;
    }
    exit(1);
}

size_t get_ncpu(int paramc, char const * paramv[]) {
    if(paramc < 2)
        return std::thread::hardware_concurrency();
    
    if(paramc > 2)
        abort_usage("Just 1 parameter is expected.");
    
    long result = -1;
    try {
        result = std::stoi(paramv[1]);

        if(result < 0)
            abort_usage("NTHREADS cannot be negative.");
    } catch (std::invalid_argument const &) {
        abort_usage("Cannot parse NTHREADS");
    }
    return result;
}

#include <fstream>
std::vector<int> read_array() {
    std::vector<int> result;
    int n;
    std::ifstream ifs("test_data/test1.txt");
    while(ifs >> n)
        result.push_back(n);
    return result;
}
