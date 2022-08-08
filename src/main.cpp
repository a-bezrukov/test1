#include <iostream>
#include <algorithm>
#include <iterator>

#include <thread>
#include <future>

#include "main.h"

int main (int paramc, char * const * paramv) {
    params_t const params = get_params(paramc, paramv);
    
    std::cout << "Threads to be used: " << params.ncpu << std::endl;
    
    if(params.do_comparison) { // Measure the threshold
        
    } else {
        data_t data = read_array(params);
        
        if(data.size() < params.ncpu) {
            std::cerr << data.size() << std::endl;
            abort_usage("The number of input data elements is less than the number of threads.");
        }
        
        if(data.size() % params.ncpu) {
            abort_usage("The number of input data elements is not divisible by ncpu.");
        }
        
        median_mutexes_t median_mutexes (params.ncpu);
        
        std::vector<std::future<std::vector<int>>> result;
        result.reserve(params.ncpu);

        for(size_t i = 0; i < params.ncpu; ++i) {
            result.emplace_back(do_math(data, i, params.ncpu, median_mutexes));
        }
        
        for(size_t i = 0; i < params.ncpu; ++i) {
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
#include <unistd.h>

void abort_usage(char const * error) {
    if(error) {
        std::cerr << "Error: " << error << std::endl;
    } else {
        std::cerr 
                << "Usage: test1 -c NTHREADS -f FILENAME -m\n"
                << "  -c NTHREADS - unsigned - the number of threads to be used.\n"
                << "    If unspecified, the test uses one thread per the available CPU.\n"
                << "  -f FILENAME - the file name to read the data from.\n"
                << "  -m - measure the size threshold.\n"
                << std::endl;
    }
    exit(1);
}

params_t get_params(int paramc, char * const * paramv) {
    params_t result {std::thread::hardware_concurrency(), {}, false};
    if(paramc < 2)
        return result;

    int opt;    
    while ((opt = getopt(paramc, paramv, "c:f:m")) != -1) {
        switch (opt) {
        case 'c':
            try {
                int ncpu = std::atoi(optarg);
                if(ncpu < 1) abort_usage("NTHREADS should be positive");
                result.ncpu = ncpu;
            } catch (std::invalid_argument const &) {
                abort_usage("Cannot parse NTHREADS");
            }
            break;
        case 'f':
            result.data_file_name = optarg;
            if(!result.data_file_name || result.data_file_name->empty()) abort_usage("FILENAME cannot be empty");
            break;
        case 'm':
            result.do_comparison = true;
            break;
        default:
            abort_usage();
        }
    }

    if (optind < paramc) {
        abort_usage("Cannot parse all the options");
    }
    
    return result;
}

#include <fstream>
std::vector<int> read_array(params_t const & params)
{
    std::vector<int> result;
    std::ifstream ifile;
    std::basic_istream<char, std::char_traits<char> > & istr = params.data_file_name ? ifile.open(params.data_file_name->c_str()), ifile : std::cin;
    int n;
    while(istr >> n)
        result.push_back(n);
    return result;
}
