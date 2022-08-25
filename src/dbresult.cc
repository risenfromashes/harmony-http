#include <cassert>
#include <libpq-fe.h>
#include <string>

#include "dbresult.h"
#include "util.h"

#include <iostream>

namespace hm::db {

#define pg_result static_cast<PGresult *>(pg_result_)

std::string_view ResultBase::name_at(int col) {
  assert(status_ == Status::MANY);
  assert(col < n_cols_);
  return PQfname(pg_result, col); //
}

std::string_view ResultBase::value_at(int row, int col) {
  assert(status_ == Status::MANY);
  assert(row < n_rows_);
  assert(col < n_cols_);
  return PQgetvalue(pg_result, row, col);
}

std::optional<std::string_view> ResultBase::get(int row, const char *name) {
  assert(status_ == Status::MANY);
  if (int col = PQfnumber(pg_result, name); col >= 0) {
    return value_at(row, col);
  }
  return std::nullopt;
}

std::optional<std::string_view> ResultBase::get(const char *name) {
  assert(status_ == Status::SINGLE || n_rows_ == 1);
  return get(0, name);
}

ResultBase::Row ResultBase::operator[](int row) { return Row(row, this); }

std::optional<std::string_view> ResultBase::operator[](const char *name) {
  return get(0, name);
}

bool ResultBase::is_error() { return status_ == ResultBase::Status::ERROR; }

std::string_view ResultBase::error_message() {
  if (error_message_) {
    // non-database error
    assert(status_ == Status::ERROR);
    return error_message_;
  } else {
    if (status_ == Status::ERROR) {
      return PQresultErrorMessage(pg_result);
    } else {
      if (status_ != Status::EMPTY && num_rows() == 0) {
        return "No record matched query";
      }
    }
  }
  return "Success";
}

ResultBase::ResultBase(void *pgres)
    : pg_result_(pgres), error_message_(nullptr) {
  auto status = PQresultStatus(pg_result);
  switch (status) {
  case PGRES_COMMAND_OK:
    status_ = Status::EMPTY;
    break;
  case PGRES_TUPLES_OK:
    status_ = Status::MANY;
    break;
  case PGRES_SINGLE_TUPLE:
    status_ = Status::SINGLE;
    break;
  default:
    status_ = Status::ERROR;
    break;
  }
  if (!is_error()) {
    n_rows_ = PQntuples(pg_result);
    n_cols_ = PQnfields(pg_result);
  }
}

ResultBase::ResultBase(std::nullptr_t) : pg_result_(nullptr) {
  status_ = Status::ERROR;
  error_message_ = "Result class in unitialised state";
}

void ResultBase::deleter(void *ptr) { PQclear(static_cast<PGresult *>(ptr)); }

std::string ResultBase::to_json() { return util::to_json(pg_result); }

bool ResultBase::exists() { return !is_error() && num_rows() > 0; }

void ResultBase::assign(const ResultBase &b) {
  pg_result_ = b.pg_result_;
  status_ = b.status_;
  n_rows_ = b.n_rows_;
  n_cols_ = b.n_rows_;
  error_message_ = b.error_message_;
}

ResultBase::ResultBase(ResultBase &&b) {
  assign(b);
  b.pg_result_ = nullptr;
}

ResultBase::ResultBase(const ResultBase &b) { assign(b); }

ResultBase::ResultBase(bool error, const char *message)
    : status_(Status::ERROR), pg_result_(nullptr), error_message_(message) {}

ResultBase &ResultBase::operator=(ResultBase &&b) {
  assign(b);
  b.pg_result_ = nullptr;
  return *this;
}

ResultBase &ResultBase::operator=(const ResultBase &b) {
  assign(b);
  return *this;
}

Result::Result(bool error, const char *message)
    : ResultBase(error, message), ptr_(nullptr, nullptr) {
  status_ = Status::ERROR;
}

Result::Result(std::nullptr_t) : ResultBase(nullptr), ptr_(nullptr, nullptr) {}

Result::Result(void *pg_result_)
    : ptr_(pg_result_, ResultBase::deleter), ResultBase(pg_result_) {}

Result::Result(Result &&b)
    : ptr_(std::move(b.ptr_)), ResultBase(std::move(b)) {}

Result &Result::operator=(Result &&b) {
  assign(b);
  b.pg_result_ = nullptr;
  ptr_ = std::move(b.ptr_);
  return *this;
}

Result::~Result() {}

SharedResult::SharedResult(bool error, const char *message)
    : ResultBase(error, message), ptr_(nullptr) {
  status_ = Status::ERROR;
}

SharedResult::SharedResult(std::nullptr_t)
    : ResultBase(nullptr), ptr_(nullptr) {}

SharedResult::SharedResult(void *pg_result_)
    : ptr_(pg_result_, ResultBase::deleter), ResultBase(pg_result_) {}

SharedResult::SharedResult(SharedResult &&b)
    : ptr_(std::move(b.ptr_)), ResultBase(std::move(b)) {}

SharedResult &SharedResult::operator=(SharedResult &&b) {
  assign(b);
  b.pg_result_ = nullptr;
  ptr_ = std::move(b.ptr_);
  return *this;
}

SharedResult::SharedResult(const SharedResult &b)
    : ResultBase(b), ptr_(b.ptr_) {}

SharedResult &SharedResult::operator=(const SharedResult &b) {
  assign(b);
  ptr_ = b.ptr_;
  return *this;
}

SharedResult::~SharedResult() {}

ResultString::ResultString(Result &&result) : result_(std::move(result)) {
  content_ = result_.value_at(0, 0);
}

ResultString &ResultString::operator=(Result &&result) {
  result_ = std::move(result);
  content_ = result_.value_at(0, 0);
  return *this;
}

ResultString::ResultString(ResultString &&result_str)
    : result_(std::move(result_str.result_)), content_(result_str.content_) {}

ResultString &ResultString::operator=(ResultString &&result) {
  result_ = std::move(result.result_);
  content_ = result.content_;
  return *this;
}

SharedResultString::SharedResultString(const SharedResult &b) : result_(b) {
  content_ = result_.value_at(0, 0);
}

SharedResultString &SharedResultString::operator=(const SharedResult &b) {
  result_ = b;
  content_ = result_.value_at(0, 0);
  return *this;
}

SharedResultString::SharedResultString(const SharedResultString &b)
    : result_(b.result_), content_(b.content_) {}

SharedResultString &SharedResultString::operator=(const SharedResultString &b) {
  result_ = b.result_;
  content_ = b.content_;
  return *this;
}

#undef pg_result

} // namespace hm::db
