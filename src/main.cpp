#include <iostream>
#include <algorithm>
#include <iterator>

#include <thread>
#include <future>

#include "main.h"

int main (int paramc, char * const * paramv) {
    params_t const params = get_params(paramc, paramv);

    if(params.do_comparison) { // Measure the threshold
        using std::chrono::high_resolution_clock;

        std::chrono::duration<double, std::nano> st_dur{0}, mt_dur{0};

        data_t data;

        std::function<void(size_t)> generate_more {[&data](size_t ncpu) {
            for(size_t i = 0; i < ncpu; ++i)
                data.emplace_back(std::rand());
        }};

        while(st_dur <= mt_dur) {
            generate_more(params.ncpu);

            auto st0 = high_resolution_clock::now();
            do_math_st(data, params.ncpu);
            st_dur = high_resolution_clock::now() - st0;

            auto mt0 = high_resolution_clock::now();
            do_math_mt(data, params.ncpu);
            mt_dur = high_resolution_clock::now() - mt0;

            std::cout << data.size() << std::endl;
        }

        std::cout << "The threshold size is: " << data.size() << std::endl;

    } else { // Read array from std::cin or file
        data_t data = read_array(params);

        if(data.size() < params.ncpu) {
            std::cerr << data.size() << std::endl;
            abort_usage("The number of input data elements is less than the number of threads.");
        }

        if(data.size() % params.ncpu) {
            abort_usage("The number of input data elements is not divisible by ncpu.");
        }

        data_t result = params.single_threaded ? do_math_st(data, params.ncpu) : do_math_mt(data, params.ncpu);

        for(int i : result)
            std::cout << i << " ";

        std::cout << std::endl;
    }
    return 0;
}

data_t do_math_mt(data_t & data, size_t ncpu) {
    data_t result;

    median_mutexes_t median_mutexes(ncpu);

    std::vector<std::future<data_t>> res_futures;
    result.reserve(ncpu);

    for(size_t i = 0; i < ncpu; ++i)
        res_futures.emplace_back(do_math_mt_(data, i, ncpu, median_mutexes));

    for(size_t i = 0; i < ncpu; ++i) {
        data_t const & cpu_res = res_futures[i].get();
        result.reserve(result.size() + cpu_res.size());
        std::copy(cpu_res.begin(), cpu_res.end(), std::back_inserter(result));
    }
    return result;
}

std::future<data_t> do_math_mt_(data_t & data, size_t cpu_num, size_t ncpu, median_mutexes_t & median_mutexes)
{
    return std::async([&data, cpu_num, ncpu, &median_mutexes]()
    {
        size_t const data_sz = data.size() / ncpu,
                median_num = data_sz / 2;

        data_t::iterator const ibeg = data.begin() + data_sz * cpu_num,
                imed = ibeg + median_num,
                iend = data.begin() + data_sz * (cpu_num + 1);

        std::nth_element(ibeg, imed, iend);

        median_mutexes[cpu_num].set_median(*imed);

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

data_t do_math_st(data_t & data, size_t ncpu) {
    data_t result;
    std::vector<int> medians(ncpu);

    size_t const data_sz = data.size() / ncpu,
            median_num = data_sz / 2;

    for(size_t cpu_num = 0; cpu_num < ncpu; ++cpu_num) {
        data_t::iterator const ibeg = data.begin() + data_sz * cpu_num,
                imed = ibeg + median_num,
                iend = data.begin() + data_sz * (cpu_num + 1);

        std::nth_element(ibeg, imed, iend);

        medians[cpu_num] = *imed;
    }

    //0-th cpu
    {
        data_t::const_iterator const ibeg = data.cbegin(),
        iend = data.cbegin() + data_sz;

        int const upper_bound = medians[1];
        std::copy_if(ibeg, iend, std::back_inserter(result), [upper_bound](int elem){return elem < upper_bound;});
    }

    //middle cpus
    for(size_t cpu_num = 1; cpu_num < ncpu - 1; ++cpu_num) {
        data_t::const_iterator const ibeg = data.cbegin() + data_sz * cpu_num,
                iend = data.cbegin() + data_sz * (cpu_num + 1);

        int const lower_bound = medians[cpu_num - 1];
        int const upper_bound = medians[cpu_num + 1];
        std::copy_if(ibeg, iend, std::back_inserter(result), [lower_bound, upper_bound](int elem){return elem > lower_bound && elem < upper_bound;});
    }

    //last cpu
    {
        data_t::const_iterator const ibeg = data.cbegin() + data_sz * (ncpu - 1),
        iend = data.cend();

        int const lower_bound = medians[ncpu - 2];
        std::copy_if(ibeg, iend, std::back_inserter(result), [lower_bound](int elem){return elem > lower_bound;});
    }
    return result;
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
                << "  -s - use single threaded code.\n"
                << std::endl;
    }
    exit(1);
}

params_t get_params(int paramc, char * const * paramv) {
    params_t result {std::thread::hardware_concurrency(), {}, false, false};
    if(paramc < 2)
        return result;

    int opt;
    while ((opt = getopt(paramc, paramv, "c:f:ms")) != -1) {
        switch (opt) {
        case 'c':
            try {
                int ncpu = std::atoi(optarg);
                if(ncpu < 1) abort_usage("NTHREADS should be 1 or more");
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
        case 's':
            result.single_threaded = true;
            break;
        default:
            abort_usage();
        }
    }

    if (optind < paramc) {
        abort_usage("Cannot parse all the options");
    }

    if(result.do_comparison) {
        if(result.data_file_name)
            abort_usage("When measuring the threshold the data is generated randomly and data file name should not be used");
        if(result.ncpu < 2)
            abort_usage("When measuring the threshold NTHREADS should be 2 or more");
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
