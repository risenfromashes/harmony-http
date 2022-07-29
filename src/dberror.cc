#include "dberror.h"

#include <libpq-fe.h>

namespace hm::db {

#define pg_result static_cast<PGresult *>(pg_result_)

std::string_view Error::message() { return PQresultErrorMessage(pg_result); }

Error::Error(void *pgres) : pg_result_(pgres) {}

Error::Error(Error &&b) : pg_result_(b.pg_result_) { b.pg_result_ = nullptr; }

Error &Error::operator=(Error &&b) {
  PQclear(pg_result);
  pg_result_ = b.pg_result_;
  b.pg_result_ = nullptr;
  return *this;
}

Error::~Error() { PQclear(pg_result); }

}; // namespace hm::db
