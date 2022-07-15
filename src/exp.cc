#include <cstring>
#include <dirent.h>

#include <ev.h>
#include <iostream>
#include <map>
#include <vector>

class FileWatcher {
public:
  FileWatcher() { loop_ = ev_loop_new(0); }
  ~FileWatcher() { ev_loop_destroy(loop_); }

  static void update_cb(struct ev_loop *loop, ev_stat *w, int revents) {
    if (w->attr.st_nlink) {
      std::cout << "[Update]" << w->path << " updated." << std::endl;
    } else {
      std::cout << "[Update]" << w->path << " deleted!" << std::endl;
    }
  }

  void list_files(std::string path) {
    DIR *dir;
    dirent *de;

    while (path.back() == ' ') {
      path.pop_back();
    }

    if (path.back() != '/') {
      path += '/';
    }

    if ((dir = opendir(path.c_str())) == nullptr) {
      std::cerr << "Couldn't open directory:" << path << ". No such directory"
                << std::endl;
      return;
    }

    while ((de = readdir(dir))) {
      if (de->d_name[0] == '.') {
        continue;
      }
      if (de->d_type & DT_DIR) {
        list_files(path + de->d_name);
      } else if (de->d_type & DT_REG | de->d_type & DT_LNK) {

        auto [itr, inserted] = files_.try_emplace(path + de->d_name, ev_stat{});
        if (inserted) {
          auto &fullname = itr->first;
          auto *w = &itr->second;
          std::cout << fullname << std::endl;
          ev_stat_init(w, update_cb, fullname.c_str(), 0.0);
          ev_stat_start(loop_, w);
        }
      }
    }
  }

  void watch() {
    std::cout << "[" << files_.size() << " files being watched]" << std::endl;
    ev_run(loop_, 0);
  }

private:
  struct ev_loop *loop_;
  std::map<std::string, ev_stat> files_;
};

int main() {
  FileWatcher fw;
  fw.list_files("..");
  fw.watch();
}
