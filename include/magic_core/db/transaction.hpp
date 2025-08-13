#pragma once

#include <sqlite_modern_cpp.h>

namespace magic_core {

class Transaction {
 public:
  explicit Transaction(sqlite::database& db, bool immediate = false)
      : db_(db), active_(true) {
    db_ << (immediate ? "BEGIN IMMEDIATE;" : "BEGIN;");
  }

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

  void commit() {
    if (active_) {
      db_ << "COMMIT;";
      active_ = false;
    }
  }

  ~Transaction() noexcept {
    if (active_) {
      try {
        db_ << "ROLLBACK;";
      } catch (...) {
      }
    }
  }

 private:
  sqlite::database& db_;
  bool active_;
};

} 


