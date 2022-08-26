#include "common_util.h"
#include "util.h"
#include "worker.h"

namespace hm::util {

uuids::uuid generate_uuid() {
  return hm::Worker::get_worker()->get_uuid_generator()->generate();
}

simdjson::ondemand::parser *get_json_parser() {
  return hm::Worker::get_worker()->get_json_parser();
}

std::string_view get_extension(std::string_view mime_type) {
  static const auto [_, dict] = util::read_mime_types();
  // trim
  mime_type = mime_type.substr(mime_type.find_first_not_of(" "));
  mime_type = mime_type.substr(0, mime_type.find_first_of(" "));

  if (auto itr = dict.find(mime_type); itr != dict.end()) {
    return itr->second;
  }

  return "";
}
}; // namespace hm::util
