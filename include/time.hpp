#ifndef TIME_HPP
#define TIME_HPP

#include <chrono>
#include "string.hpp"

// Return the current time [seconds]
double now() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count()*1e-6;
}

// Return the current date [yymmdd]
std::string today() {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << strn(tm.tm_year+1900,4) << strn(tm.tm_mon+1, 2) << strn(tm.tm_mday,2);
    return ss.str();
}

// Converts an ammount of time [seconds] to a formatted string hh:mm:ss
template<typename T>
std::string time_str(T t) {
    std::string date;
    
    if (t < 1.0) {
        int mag = floor(log10(t));
        if (mag >= -3) {
            return strn(round(t*1e3))+"ms";
        } else if (mag >= -6) {
            return strn(round(t*1e6))+"us";
        } else {
            return strn(round(t*1e9))+"ns";
        }
    } else {
        std::size_t day  = floor(t/(24*60*60));
        std::size_t hour = floor(t/(60*60)) - day*24;
        std::size_t min  = floor(t/60) - day*24*60 - hour*60;
        std::size_t sec  = floor(t) - day*24*60*60 - hour*60*60 - min*60;
        
        if (day  != 0) date += strn(day)+'d';
        if (hour != 0) date += strn(hour,2)+'h';
        if (min  != 0) date += strn(min,2)+'m';
        date += strn(sec,2)+'s';
        
        if (date[0] == '0' && date.size() != 2) {
            date.erase(0,1);
        }
    }
    
    return date;
}

// Converts an ammount of time [seconds] to a formatted string ss:ms:us:ns
template<typename T>
std::string seconds_str(T t) {
    std::string date;

    std::size_t sec = floor(t);
    std::size_t ms  = floor((t - sec)*1000);
    std::size_t us  = floor(((t - sec)*1000 - ms)*1000);
    std::size_t ns  = floor((((t - sec)*1000 - ms)*1000 - us)*1000);
        
    if (sec != 0)                  date += strn(sec)+"s";
    if (ms  != 0 || !date.empty()) date += strn(ms,3)+"ms";
    if (us  != 0 || !date.empty()) date += strn(us,3)+"us";
    date += strn(us,3)+"ns";
    
    while (date[0] == '0' && date.size() != 3) {
        date.erase(0,1);
    }
    
    return date;
}

// Execute the provided code and return the time it took to execute [seconds]
template<typename F>
double profile(F&& func) {
    auto start = now();
    func();
    return now() - start;
}

// Execute 'n' times the provided code and return the time it took to execute [seconds]
template<typename F>
double profile(F&& func, uint_t n) {
    auto start = now();
    for (uint_t i = 0; i < n; ++i) {
        func();
    }
    return now() - start;
}

struct progress_t {
    double start;
    std::size_t i = 0;
    std::size_t n;
    std::size_t max_length = 0;
};

// Begin timing an iterative process of 'n' iterations
progress_t progress_start(std::size_t n) {
    progress_t r;
    r.start = now();
    r.n = n;
    return r;
}

void progress_(progress_t& p) {
    double total = now() - p.start;
    double remaining = total*double(p.n)/(p.i+1) - total;

    const std::size_t ndash = 50;
    std::string msg;
    // Progress bar
    msg += "["+std::string(floor(ndash*(p.i+1)/double(p.n)),'-')
        + std::string(ndash - floor(ndash*(p.i+1)/double(p.n)),' ')+"] ";
    // Iteration count
    msg += strn((p.i+1), floor(log10(double(p.n))) + 1, ' ')+" ";
    // Percentage
    msg += strn(std::size_t(floor(100.0*(p.i+1)/double(p.n))), 3, ' ')+"%, ";
    // Timings
    msg += time_str(total)+" elapsed, "+time_str(remaining)+" left, "
        + time_str(total+remaining)+" total";
    // Fill with spaces
    p.max_length = std::max(p.max_length, msg.size());
    msg += std::string(p.max_length - msg.size(), ' ');

    std::cout << "\r" << msg << std::flush;
}

// Updates a progress bar for an iterative process ('p' is created from 'progress_start')
template<typename M = uint_t>
void progress(progress_t& p, const M& mod = 1) {
    if (p.i % mod == 0 || p.i == p.n-1) {
        progress_(p);
    }

    ++p.i;

    if (p.i >= p.n) {
        std::cout << std::endl;
    }
}

// Updates a progress bar for an iterative process ('p' is created from 'progress_start')
// 'i' is the current iteration number.
template<typename I, typename M = uint_t>
void print_progress(progress_t& p, const I& ti, const M& mod = 1) {
    p.i = ti;
    if (p.i >= p.n-1) {
        p.i = p.n-1;
        progress_(p);
        std::cout << std::endl;
    } else if (p.i % mod == 0) {
        progress_(p);
    }
}

#endif
