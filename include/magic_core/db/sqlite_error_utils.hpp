#pragma once

#include <string>
#include <sqlite3.h>
#include <sqlite_modern_cpp.h>

namespace magic_core {

enum class DbErrorKind {
  BusyOrLocked,
  Constraint,
  Readonly,
  Io,
  CantOpen,
  Full,
  Schema,
  Generic
};

inline DbErrorKind classify_sqlite_code(int primary_code) {
  switch (primary_code) {
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
      return DbErrorKind::BusyOrLocked;
    case SQLITE_CONSTRAINT:
      return DbErrorKind::Constraint;
    case SQLITE_READONLY:
      return DbErrorKind::Readonly;
    case SQLITE_IOERR:
      return DbErrorKind::Io;
    case SQLITE_CANTOPEN:
      return DbErrorKind::CantOpen;
    case SQLITE_FULL:
      return DbErrorKind::Full;
    case SQLITE_ERROR:
    case SQLITE_SCHEMA:
      return DbErrorKind::Schema;
    default:
      return DbErrorKind::Generic;
  }
}

inline std::string kind_to_string(DbErrorKind kind) {
  switch (kind) {
    case DbErrorKind::BusyOrLocked: return "busy_or_locked";
    case DbErrorKind::Constraint: return "constraint";
    case DbErrorKind::Readonly: return "readonly";
    case DbErrorKind::Io: return "io";
    case DbErrorKind::CantOpen: return "cantopen";
    case DbErrorKind::Full: return "full";
    case DbErrorKind::Schema: return "schema";
    default: return "generic";
  }
}

inline std::string format_db_error(const std::string& operation, const sqlite::sqlite_exception& e) {
  const int code = e.get_code();
  const int xcode = e.get_extended_code();
  const DbErrorKind kind = classify_sqlite_code(code);
  std::string msg = operation + " failed: (" + kind_to_string(kind) + ") " + e.errstr();
  msg += " [code=" + std::to_string(code) + ", xcode=" + std::to_string(xcode) + "]";
  return msg;
}

} // namespace magic_core


