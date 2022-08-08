#include <cassert>
#include <libpq-fe.h>
#include <string>

#include "dbresult.h"
#include "util.h"

namespace hm::db {

#define pg_result static_cast<PGresult *>(pg_result_)

std::string_view Result::name_at(int col) {
  assert(status_ == Status::MANY);
  assert(col < n_cols_);
  return PQfname(pg_result, col); //
}

std::string_view Result::value_at(int row, int col) {
  assert(status_ == Status::MANY);
  assert(row < n_rows_);
  assert(col < n_cols_);
  return PQgetvalue(pg_result, row, col);
}

std::optional<std::string_view> Result::get(int row, const char *name) {
  assert(status_ == Status::MANY);
  if (int col = PQfnumber(pg_result, name); col >= 0) {
    return value_at(row, col);
  }
  return std::nullopt;
}

std::optional<std::string_view> Result::get(const char *name) {
  assert(status_ == Status::SINGLE || n_rows_ == 1);
  return get(0, name);
}

Result::Row Result::operator[](int row) { return Row(row, this); }

bool Result::is_error() { return status_ == Result::Status::ERROR; }

std::string_view Result::error_message() {
  assert(status_ == Status::ERROR);
  return PQresultErrorMessage(pg_result);
}

Result::Result(void *pgres) : pg_result_(pgres) {
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
  n_cols_ = PQntuples(pg_result);
  n_rows_ = PQnfields(pg_result);
}

Result::Result(Result &&b) : pg_result_(b.pg_result_) {
  n_rows_ = b.n_rows_;
  n_cols_ = b.n_rows_;
  b.pg_result_ = nullptr;
}

Result &Result::operator=(Result &&b) {
  if (pg_result_) {
    PQclear(pg_result);
  }
  pg_result_ = b.pg_result_;
  n_rows_ = b.n_rows_;
  n_cols_ = b.n_cols_;
  b.pg_result_ = nullptr;
  return *this;
}

std::string Result::to_json() { return util::to_json(pg_result); }

Result::~Result() {
  if (pg_result_) {
    PQclear(pg_result);
  }
}

#undef pg_result

} // namespace hm::db
