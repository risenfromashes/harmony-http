#include "httprequest.h"
#include "stream.h"

namespace hm {

std::optional<std::string_view>
HttpRequest::get_header(std::string_view header_name) {
  return stream_->headers.get_header(header_name);
}
std::string_view HttpRequest::path() { return stream_->path_; }
std::string_view HttpRequest::query() { return stream_->query_; }

}; // namespace hm
