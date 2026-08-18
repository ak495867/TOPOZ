#pragma once

#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace topos {

double now_seconds();
double clamp(double value, double low, double high);
double mean(const std::vector<double>& values);
double standard_deviation(const std::vector<double>& values);
double percentile(std::vector<double> values, double p);
std::string uppercase(std::string value);
std::string format_number(double value, int precision = 3);
std::string bar(double value, std::size_t width = 16, double maximum = 1.0);

}
