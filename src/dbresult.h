#pragma once

#include <string_view>

namespace hm::db {

class Result {
  friend class Session;

public:
  class Row {
    friend class Result;
    friend class Iterator;

  public:
    std::string_view name_at(int index) {
      return result_->name_at(index); //
    }
    std::string_view value_at(int index) {
      return result_->value_at(row_index_, index);
    }

    std::string_view get(const char *name) {
      return result_->get(row_index_, name);
    }

  private:
    Row(int row, Result *result) : row_index_(row), result_(result) {}

    Result *result_;
    int row_index_;
  };

  class Iterator {
    friend class Result;

    Iterator &operator+(int n) {
      row_.row_index_ = std::min(row_.row_index_ + n, row_.result_->n_rows_);
      return *this;
    }

    Iterator &next() { return (*this + 1); }
    Iterator &operator++() { return next(); }
    Iterator &operator++(int) { return next(); }

    Row &operator*() { return row_; }
    Row *operator->() { return &row_; }

    friend bool operator==(const Iterator &a, const Iterator &b) {
      return a.row_.row_index_ == b.row_.row_index_;
    }

  private:
    Iterator(int row, Result *result) : row_(row, result) {}
    Row row_;
  };

  Iterator begin() { return Iterator(0, this); }
  Iterator end() { return Iterator(n_rows_, this); }

  int num_rows() { return n_rows_; }
  int num_cols() { return n_cols_; }

  std::string_view name_at(int col);
  std::string_view value_at(int row, int col);

  std::string_view get(int row, const char *name);

  std::string_view error_message();

  Result(void *pg_result);
  Result(Result &&b);
  Result &operator=(Result &b);

  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;
  ~Result();

private:
  bool is_error_;
  void *pg_result_;
  int n_rows_;
  int n_cols_;
};

}; // namespace hm::db
