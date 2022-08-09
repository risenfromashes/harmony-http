#include "common_util.h"
#include "worker.h"

uuids::uuid generate_uuid() {
  return hm::Worker::get_worker()->get_uuid_generator()->generate();
}

simdjson::ondemand::parser *get_json_parser() {
  return hm::Worker::get_worker()->get_json_parser();
}
