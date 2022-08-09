#include "json.h"

#include <iostream>

using namespace hm;
int main() {
  json::Writer writer;
  {
    json::Object root = writer.root();
    root["name"] = "siam";
    root["type"] = "retard";
    {
      json::Array children = root["children"];
      for (int i = 0; i < 10; i++) {
        json::Object child = children.next_object();
        child["index"] = i;
        child["age"] = 50 - i;
        child["name"] = "child" + std::to_string(i);
        child["retarded"] = true;
        child["child"] = nullptr;
      }
    }
  }
  std::cout << writer.string() << std::endl;
}

