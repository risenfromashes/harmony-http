
#include <chrono>
#include <iostream>
#include <vector>

long elapsed(const std::chrono::high_resolution_clock::time_point &start) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::high_resolution_clock::now() - start)
      .count();
}

int main() {
  std::string name = "Name";
  auto t = std::chrono::high_resolution_clock::now();
  std::vector<std::string> vec;
  for (int t = 0; t < 100000; t++) {
    std::string ret;
    // ret.reserve(20);
    ret += "<html>";
    for (int i = 0; i < 10; i++) {
      ret += "<p>";
      ret += name;
      ret += ' ';
      ret += std::to_string(i);
      ret += "</p>";
    }
    ret += "</html>";
    vec.emplace_back(std::move(ret));
  }
  auto time = elapsed(t);

  std::cout << vec[108] << std::endl;
  std::cout << "elapsed: " << time / 100000.0 << " ns" << std::endl;
}
