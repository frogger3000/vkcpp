#include "util.h"
#include <iomanip>
#include <sstream>

std::string metrics::utils::quoted(std::string s) {
  std::stringstream ss;
  ss << std::quoted(s);
  return ss.str();
}
