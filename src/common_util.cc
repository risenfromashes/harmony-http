#include "common_util.h"
#include "util.h"
#include "worker.h"
#include <fstream>

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

static char encoding_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

static int mod_table[] = {0, 2, 1};

std::string base64_encode(const unsigned char *data, size_t input_length) {

  size_t output_length = 4 * ((input_length + 2) / 3);
  std::string encoded_data(output_length, '\0');

  for (int i = 0, j = 0; i < input_length;) {

    uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
    uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
    uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
    encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
  }

  for (int i = 0; i < mod_table[input_length % 3]; i++)
    encoded_data[output_length - 1 - i] = '=';

  return encoded_data;
}

bool base64_decode_to_file(std::string_view data_, const std::string &outfile) {
  unsigned char *data = (unsigned char *)data_.data();
  size_t input_length = data_.length();

  static bool built_table = false;
  static char decoding_table[256];
  if (!built_table) {
    for (int i = 0; i < 64; i++) {
      decoding_table[(unsigned char)encoding_table[i]] = i;
    }
    built_table = true;
  }

  if (input_length % 4 != 0)
    return false;

  size_t output_length = input_length / 4 * 3;
  if (data[input_length - 1] == '=') {
    output_length--;
  }
  if (data[input_length - 2] == '=') {
    output_length--;
  }

  std::ofstream out(outfile);

  for (int i = 0, j = 0; i < input_length;) {

    uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
    uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
    uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
    uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

    uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) +
                      (sextet_c << 1 * 6) + (sextet_d << 0 * 6);

    if (j < output_length) {
      out.put((triple >> 2 * 8) & 0xFF);
    }
    if (j < output_length) {
      out.put((triple >> 1 * 8) & 0xFF);
    }
    if (j < output_length) {
      out.put((triple >> 0 * 8) & 0xFF);
    }
  }
  out.close();
  return true;
}

}; // namespace hm::util
