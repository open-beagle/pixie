#include "src/stirling/utils/byte_format.h"
#include <glog/logging.h>

namespace pl {
namespace stirling {
namespace utils {

int LEStrToInt(const std::string_view str) {
  DCHECK(str.size() <= sizeof(int));
  uint32_t result = 0;
  for (size_t i = 0; i < str.size(); i++) {
    result = static_cast<unsigned char>(str[str.size() - 1 - i]) + (result << 8);
  }
  return result;
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
