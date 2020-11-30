#ifndef PULSAR_HPP
#define PULSAR_HPP
#include <vector>
#include <complex>
#include <functional>

double default_profile(double p);

std::tuple<std::function<void(std::vector<std::complex<double>>&)>, size_t> get_pulsar(double fmin_MHz, double fmax_MHz, double period_ms, double dm, size_t nperiods, const std::function<double(double)> profile=default_profile);

#endif
