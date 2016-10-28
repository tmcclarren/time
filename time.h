#pragma once

#include <sys/time.h>
#include <iomanip>
#include <iostream>
#include <string.h>

const unsigned long MAX_USECS = 1000000L;

// Time class to provide way to manipulate times with normal operators.
// When working with the innards of this class, it's useful to know
// that on Linux, things are defined like this:
//
// struct timespec {
//     tv_sec:       time_t
//     tv_nsec:      long int
// }
//
// struct timeval {
//     tv_sec:       time_t
//     suseconds_t:  long int
// }
//
// time_t:           long int
//
// Operators for this class are not optimized... I wrote them to (hopefully)
// have less likelihood of overflowing, versus trying to make them fast.
// Keep that in mind if you are using them a lot.

class Time {
public:
    Time() {
        _t.tv_sec = 0;
        _t.tv_usec = 0;
    }

    Time(const Time& t) {
        _t.tv_sec = t._t.tv_sec;
        _t.tv_usec = t._t.tv_usec;
    }

    Time(const timeval& t) {
        memcpy(&_t, &t, sizeof(timeval));
    }

    explicit Time(unsigned long micros) {
        _t.tv_sec = (micros / MAX_USECS);
        _t.tv_usec = (micros % MAX_USECS);
    }

    explicit Time(const time_t secondsSinceEpoch, suseconds_t micros = 0) {
        _t.tv_sec = secondsSinceEpoch;
        _t.tv_usec = micros;
    }

    ~Time() { }

    struct tm* localtime() const {
        return ::localtime(&_t.tv_sec);
    }
    
    unsigned int hour() const {
        return localtime()->tm_hour;
    }

    unsigned int minute() const {
        return localtime()->tm_min;
    }

    unsigned int second() const {
        return localtime()->tm_sec;
    }

    unsigned int day() const {
        return localtime()->tm_mday;
    }

    unsigned int month() const {
        return localtime()->tm_mon;
    }

    unsigned int year() const {
        return localtime()->tm_year;
    }

    Time& operator = (const Time& rhs) {
        this->_t = rhs._t; return *this;
    }
    
    bool  operator == (const Time& rhs) const { return in_micros() == rhs.in_micros(); }
    bool  operator != (const Time& rhs) const { return !(*this == rhs); }
    bool  operator >= (const Time& rhs) const { return in_micros() >= rhs.in_micros(); }
    bool  operator <= (const Time& rhs) const { return in_micros() <= rhs.in_micros(); }
    bool  operator > (const Time& rhs) const { return in_micros() > rhs.in_micros(); }
    bool  operator < (const Time& rhs) const { return in_micros() < rhs.in_micros(); }

    Time& now() {
        gettimeofday(&_t, NULL);
        return *this;
    }

    Time& future(int secondsInFuture) {
        gettimeofday(&_t, NULL);
        _t.tv_sec += secondsInFuture;
        return *this;
    }

    Time& past(int secondsInPast) {
        gettimeofday(&_t, NULL);
        _t.tv_sec -= secondsInPast;
        return *this;
    }

    Time& operator += (const Time& rhs) {
        struct timeval result;
        timeradd(&_t, &rhs._t, &result);
        _t.tv_sec = result.tv_sec;
        _t.tv_usec = result.tv_usec;
        return *this;
    }

    Time& operator -= (const Time& rhs) {
        struct timeval result;
        timersub(&_t, &rhs._t, &result);
        _t.tv_sec = result.tv_sec;
        _t.tv_usec = result.tv_usec;
        return *this;
    }

    Time& operator /= (int denominator) {
        Time r;

        if (_t.tv_sec >= denominator) {
            r._t.tv_sec = _t.tv_sec / denominator;
            unsigned long remainder = _t.tv_sec % denominator;
            unsigned long micros = (remainder * MAX_USECS) + _t.tv_usec;
            r._t.tv_usec = micros / denominator;
        }
        else {
            r._t.tv_sec = 0L;
            unsigned long micros = (_t.tv_sec * MAX_USECS) + _t.tv_usec;
            r._t.tv_usec = micros / denominator;
        }
        *this = r;
        return *this;
    }

    Time& operator *= (int multiplier) {
        Time r;
        int secsToAdd = 0;
        unsigned long micros = _t.tv_usec * multiplier;
        if (micros > MAX_USECS) {
            secsToAdd = (micros / MAX_USECS);
            r._t.tv_usec = (micros % MAX_USECS);
        }
        else {
            r._t.tv_usec = micros;
        }
        r._t.tv_sec *= multiplier;
        r._t.tv_sec += secsToAdd;
        *this = r;
        return *this;
    }

    // seconds since the epoch
    time_t seconds() const { return _t.tv_sec; }
    suseconds_t millis() const { return _t.tv_usec / 1000; }
    suseconds_t micros() const { return _t.tv_usec; }
    suseconds_t in_micros() const { return ((_t.tv_sec * MAX_USECS) + _t.tv_usec); }

    friend Time operator + (const Time& t1, const Time& t2) {
        Time r = t1;
        r += t2;
        return r;
    }

    friend Time operator + (const Time& t1, unsigned long offset) {
        Time r(offset);
        return t1 + r;
    }

    friend Time operator - (const Time& t1, const Time& t2) {
       Time r = t1;
       r -= t2;
       return r;
    }

    friend Time operator - (const Time& t1, unsigned long offset) {
        Time r(offset);
        return t1 - r;
    }

    friend Time operator / (const Time& t, unsigned int denominator) {
        Time r = t;
        r /= denominator;
        return r;
    }

    friend Time operator * (const Time& t, unsigned int multiplier) {
        Time r = t;
        r *= multiplier;
        return r;
    }

    friend std::ostream& operator << (std::ostream& os, const Time& t) {
        os << t._t.tv_sec << '.' << std::setw(6) << std::right << std::setfill('0') <<
            t._t.tv_usec << 's' << std::setfill(' ') << std::left;
        return os;
    }

private:
    friend class Duration;
    struct timeval _t;
};

class Duration {
public:
    explicit Duration(const Time& t, bool showMicros = false): _t(t._t), _su(showMicros) { }
    
    friend std::ostream& operator << (std::ostream& os, const Duration& d) {
        static const unsigned long MINUTE = 60L;
        static const unsigned long HOUR = MINUTE * 60L;
        static const unsigned long DAY = HOUR * 24L;

        time_t s = d._t.tv_sec;
        if (s / DAY) {
            os << s/DAY << "d ";
            s = s % DAY;
        }
        os << std::right;
        os << std::setfill('0') << s / HOUR << ':';
        s = s % HOUR;
        os << std::setw(2) << s / MINUTE << ':';
        s = s % MINUTE;
        os << std::setw(2) << s;
        if (d._su) {
            os << '.' << std::setw(6) << std::setfill('0') << d._t.tv_usec;
        }
        os << std::setfill(' ');
        return os;
    }

private:
    struct timeval _t;
    bool _su;
};

