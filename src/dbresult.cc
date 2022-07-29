#include "dbresult.h"
#include <cassert>
#include <libpq-fe.h>

namespace hm::db {

#define pg_result static_cast<PGresult *>(pg_result_)

std::string_view Result::name_at(int col) {
  return PQfname(pg_result, col); //
}

std::string_view Result::value_at(int row, int col) {
  return PQgetvalue(pg_result, row, col);
}

std::string_view Result::get(int row, const char *name) {
  return PQgetvalue(pg_result, row, PQfnumber(pg_result, name));
}

Result::Result(void *pgres) : pg_result_(pgres) {
  auto status = PQresultStatus(pg_result);
  n_cols_ = PQntuples(pg_result);
  n_rows_ = PQnfields(pg_result);
}

Result::Result(Result &&b) : pg_result_(b.pg_result_) {
  n_rows_ = b.n_rows_;
  n_cols_ = b.n_rows_;
  b.pg_result_ = nullptr;
}

Result &Result::operator=(Result &b) {
  if (pg_result_) {
    PQclear(pg_result);
  }
  pg_result_ = b.pg_result_;
  n_rows_ = b.n_rows_;
  n_cols_ = b.n_cols_;
  b.pg_result_ = nullptr;
  return *this;
}

Result::~Result() { PQclear(pg_result); }

#undef pg_result

} // namespace hm::db
