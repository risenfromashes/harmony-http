#pragma once

#include <memory>
#include <optional>
#include <string_view>

namespace hm::db {

class ResultBase {
  friend class Session;

public:
  enum class Status { ERROR, EMPTY, SINGLE, MANY };

  class Iterator;
  class Row {
    friend class ResultBase;
    friend class Iterator;

  public:
    std::string_view name_at(int index) {
      return result_->name_at(index); //
    }
    std::string_view value_at(int index) {
      return result_->value_at(row_index_, index);
    }

    std::optional<std::string_view> get(const char *name) {
      return result_->get(row_index_, name);
    }

    std::optional<std::string_view> operator[](const char *name) {
      return get(name);
    }

    std::optional<std::string_view> operator[](unsigned int index) {
      return result_->value_at(row_index_, index);
    }

    operator std::string_view() { return result_->value_at(row_index_, 0); }

    friend bool operator==(const Iterator &a, const Iterator &b);

  private:
    Row(int row, ResultBase *result) : row_index_(row), result_(result) {}

    ResultBase *result_;
    int row_index_;
  };

  class Iterator {
    friend class ResultBase;

  public:
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
      return a.row_.result_ == b.row_.result_ &&
             a.row_.row_index_ == b.row_.row_index_;
    }

  private:
    Iterator(int row, ResultBase *result) : row_(row, result) {}
    Row row_;
  };

  Iterator begin() { return Iterator(0, this); }
  Iterator end() { return Iterator(n_rows_, this); }

  int num_rows() { return n_rows_; }
  int num_cols() { return n_cols_; }

  std::string_view name_at(int col);
  std::string_view value_at(int row, int col);

  std::optional<std::string_view> get(int row, const char *name);
  std::optional<std::string_view> get(const char *name);

  Row operator[](int row);
  std::optional<std::string_view> operator[](const char *name);

  bool is_error();
  std::string_view error_message();

  std::string to_json();

  bool exists();

  operator bool() {
    return !is_error() && (status_ == Status::EMPTY || n_rows_ > 0);
  }
  // Error result
protected:
  void assign(const ResultBase &b);

  ResultBase(bool error, const char *message);
  ResultBase(std::nullptr_t);
  ResultBase(void *pg_result);

  ResultBase(ResultBase &&b);
  ResultBase &operator=(ResultBase &&b);

  ResultBase(const ResultBase &b);
  ResultBase &operator=(const ResultBase &b);

  static void deleter(void *ptr);

protected:
  void *pg_result_ = nullptr;
  Status status_;
  int n_rows_;
  int n_cols_;

  const char *error_message_;
};

class Result : public ResultBase {
public:
  Result(bool error, const char *message);
  Result(std::nullptr_t);
  Result(void *pg_result);

  Result(Result &&b);
  Result &operator=(Result &&b);

  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;
  ~Result();

private:
  std::unique_ptr<void, void (*)(void *)> ptr_;
};

class SharedResult : public ResultBase {
public:
  SharedResult(bool error, const char *message);
  SharedResult(std::nullptr_t);
  SharedResult(void *pg_result);

  SharedResult(SharedResult &&b);
  SharedResult &operator=(SharedResult &&b);

  SharedResult(const SharedResult &);
  SharedResult &operator=(const SharedResult &);
  ~SharedResult();

private:
  std::shared_ptr<void> ptr_;
};

class ResultString {
public:
  ResultString(Result &&result);
  ResultString &operator=(Result &&result);

  ResultString(ResultString &&result);
  ResultString &operator=(ResultString &&result);

  ResultString(const ResultString &) = delete;
  ResultString &operator=(const ResultString &) = delete;

  operator std::string_view() const { return content_; }
  std::string_view get() const { return content_; }
  size_t length() const { return content_.length(); }
  std::string_view substr(std::size_t pos) const {
    return content_.substr(pos);
  }

private:
  std::string_view content_;
  Result result_;
};

class SharedResultString {
public:
  SharedResultString(const SharedResult &result);
  SharedResultString &operator=(const SharedResult &result);

  SharedResultString(const SharedResultString &);
  SharedResultString &operator=(const SharedResultString &);

  operator std::string_view() const { return content_; }
  std::string_view get() const { return content_; }
  size_t length() const { return content_.length(); }
  std::string_view substr(std::size_t pos) const {
    return content_.substr(pos);
  }

private:
  std::string_view content_;
  SharedResult result_;
};

}; // namespace hm::db
