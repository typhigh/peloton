//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// constraints_test.cpp
//
// Identification: test/catalog/constraints_test.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#define VALUE_TESTS

#include <include/tcop/tcop.h>
#include <include/planner/create_plan.h>
#include <include/executor/executors.h>
#include "gtest/gtest.h"

#include "concurrency/testing_transaction_util.h"
#include "common/harness.h"

#include "catalog/schema.h"
#include "catalog/foreign_key.h"
#include "type/value.h"
#include "concurrency/transaction.h"
#include "concurrency/transaction_manager_factory.h"
#include "executor/abstract_executor.h"
#include "storage/tile_group_factory.h"
#include "storage/tuple.h"
#include "storage/table_factory.h"
#include "storage/database.h"
#include "index/index_factory.h"
#include "catalog/constraints_tests_util.h"

#include "common/harness.h"
#include "common/macros.h"
#include "common/logger.h"
#include "optimizer/simple_optimizer.h"
#include "executor/logical_tile_factory.h"
#include "executor/plan_executor.h"
#include "executor/seq_scan_executor.h"
#include "executor/update_executor.h"
#include "planner/plan_util.h"
#include "parser/postgresparser.h"

#define NOTNULL_TEST
#define MULTI_NOTNULL_TEST
#define CHECK_TEST
#define DEFAULT_TEST
//#define PRIMARY_UNIQUEKEY_TEST
#define FOREIGHN_KEY_TEST
#define FOREIGHN_MULTI_KEY_TEST
#define UNIQUE_TEST
#define MULTI_UNIQUE_TEST

#define DEFAULT_VALUE 11111

namespace peloton {
namespace test {

//===--------------------------------------------------------------------===//
// Constraints Tests
//===--------------------------------------------------------------------===//

class ConstraintsTests : public PelotonTest {};

// FIXME: see the explanation rpc_client_test and rpc_server_test
#ifdef NOTNULL_TEST
TEST_F(ConstraintsTests, NOTNULLTest) {
  // First, generate the table with index
  // this table has 15 rows:
  //  int(primary)  int   double  var(22) (unique)
  //  0             1     2       "3"
  //  10            11    12      "13"
  //  20            21    22      "23"
  //  .....
  //  140           141   142     "143"

  ConstraintsTestsUtil::CreateAndPopulateTable();
  std::unique_ptr<storage::DataTable> data_table(
      ConstraintsTestsUtil::CreateAndPopulateTable());

  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();

  // begin this transaction
  auto txn = txn_manager.BeginTransaction();

  // two columns are both NULL
  bool hasException = false;

  try {
    ConstraintsTestsUtil::ExecuteInsert(
        txn, data_table.get(),
        type::ValueFactory::GetNullValueByType(type::Type::INTEGER),
        type::ValueFactory::GetIntegerValue(
            ConstraintsTestsUtil::PopulatedValue(15, 1)),
        type::ValueFactory::GetIntegerValue(
            ConstraintsTestsUtil::PopulatedValue(15, 2)),
        type::ValueFactory::GetVarcharValue(
            std::to_string(ConstraintsTestsUtil::PopulatedValue(15, 3))));

  } catch (ConstraintException e) {
    hasException = true;
  }
  EXPECT_TRUE(hasException);

  auto column1 = catalog::Column(type::Type::INTEGER, 25, "A", false, 0);
  auto constraints = catalog::Constraint(ConstraintType::CHECK, "check1");
  type::Value tmp_value = type::Value(type::Type::INTEGER, 0);
  constraints.AddCheck(ExpressionType::COMPARE_GREATERTHAN, tmp_value);
  column1.AddConstraint(constraints);
  LOG_DEBUG("**** %s", constraints.GetInfo().c_str());
  catalog::Schema *table_schema = new catalog::Schema({column1});
  std::string table_name("TEST_TABLE");
  bool own_schema = true;
  bool adapt_table = false;
  storage::DataTable *table = storage::TableFactory::GetDataTable(
      INVALID_OID, INVALID_OID, table_schema, table_name,
      TESTS_TUPLES_PER_TILEGROUP, own_schema, adapt_table);
  std::unique_ptr<storage::DataTable> data_table(table);

  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();

  // begin this transaction
  auto txn = txn_manager.BeginTransaction();
  // Test1: insert a tuple with column  meet the constraint requirment
  bool hasException = false;
  try {
    ConstraintsTestsUtil::ExecuteOneInsert(
        txn, data_table.get(), type::ValueFactory::GetIntegerValue(10));
  } catch (ConstraintException e) {
    hasException = true;
  }
  EXPECT_FALSE(hasException);

  // Test2: insert not a valid column violate the constraint
  hasException = false;
  try {
    ConstraintsTestsUtil::ExecuteOneInsert(
        txn, data_table.get(), type::ValueFactory::GetIntegerValue(-1));
  } catch (ConstraintException e) {
    hasException = true;
  }
  EXPECT_TRUE(hasException);

  // commit this transaction
  txn_manager.CommitTransaction(txn);
  delete data_table.release();
}
#endif

#ifdef UNIQUE_TEST
TEST_F(ConstraintsTests, UNIQUETest) {
  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();
  auto catalog = catalog::Catalog::GetInstance();
  auto txn = txn_manager.BeginTransaction();
  catalog->CreateDatabase(DEFAULT_DB_NAME, nullptr);
  auto column1 = catalog::Column(type::Type::INTEGER, 25, "A", false, 0);
  auto column2 = catalog::Column(type::Type::INTEGER, 25, "B", false, 1);

  auto constraints = catalog::Constraint(ConstraintType::UNIQUE, "unique1");
  column1.AddConstraint(constraints);
  LOG_DEBUG("**** %s", constraints.GetInfo().c_str());
  std::unique_ptr<catalog::Schema> table_schema(
      new catalog::Schema({column1, column2}));
  std::string table_name("TEST_TABLE");
  catalog::Catalog::GetInstance()->CreateTable(DEFAULT_DB_NAME, table_name,
                                               std::move(table_schema), txn);
  txn_manager.CommitTransaction(txn);
  storage::Database *database = catalog->GetDatabaseWithName(DEFAULT_DB_NAME);
  storage::DataTable *table = database->GetTableWithName(table_name);

  // table->AddUNIQUEIndex();

  txn = txn_manager.BeginTransaction();
  // begin this transaction
  // Test1: insert a tuple with column  meet the unique requirment
  bool hasException = false;
  try {
    // bool result = true;
    // result =
    ConstraintsTestsUtil::ExecuteOneInsert(
        txn, table, type::ValueFactory::GetIntegerValue(10));
    // if (result == false) hasException = true;
  } catch (ConstraintException e) {
    hasException = true;
  }
  EXPECT_FALSE(hasException);

  // Test2: insert not a valid column violate the constraint
  hasException = false;
  try {
    // bool result = true;
    // result =
    ConstraintsTestsUtil::ExecuteOneInsert(
        txn, table, type::ValueFactory::GetIntegerValue(10));
    // if (result == false) hasException = true;
  } catch (ConstraintException e) {
    hasException = true;
  }
  EXPECT_TRUE(hasException);

  hasException = false;
  try {
    ConstraintsTestsUtil::ExecuteOneInsert(
        txn, table, type::ValueFactory::GetIntegerValue(20));
  } catch (ConstraintException e) {
    hasException = true;
  }
  EXPECT_FALSE(hasException);

  // commit this transaction
  txn_manager.CommitTransaction(txn);

  delete data_table.release();

}
#endif
/*
#ifdef PRIMARY_UNIQUEKEY_TEST
TEST_F(ConstraintsTests, CombinedPrimaryKeyTest) {
  // First, generate the table with index
  // this table has 10 rows:
  //  int(primary)  int(primary)
  //  0             0
  //  1             1
  //  2             2
  //  .....
  //  9             9

  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();

  {
    std::unique_ptr<storage::DataTable> data_table(
        TransactionTestsUtil::CreateCombinedPrimaryKeyTable());
    // Test1: insert 2 tuple with duplicated primary key
    // txn1: insert (0, 1) -- success
    // txn0 commit
    // txn1: insert (1, 1) -- fail
    // txn1 commit
    TransactionScheduler scheduler(2, data_table.get(), &txn_manager);
    scheduler.Txn(0).Insert(0, 1);
    scheduler.Txn(0).Commit();
    scheduler.Txn(1).Insert(1, 1);
    scheduler.Txn(1).Commit();

    scheduler.Run();

    EXPECT_TRUE(ResultType::SUCCESS == scheduler.schedules[0].txn_result);
    EXPECT_TRUE(ResultType::ABORTED == scheduler.schedules[1].txn_result);
  }
}

TEST_F(ConstraintsTests, MultiTransactionUniqueConstraintsTest) {
  // First, generate the table with index
  // this table has 10 rows:
  //  int(primary)  int(unique)
  //  0             0
  //  1             1
  //  2             2
  //  .....
  //  9             9

  auto &txn_manager = concurrency::TransactionManagerFactory::GetInstance();

  {
    std::unique_ptr<storage::DataTable> data_table(
        TransactionTestsUtil::CreatePrimaryKeyUniqueKeyTable());
    // Test1: insert 2 tuple with duplicated primary key
    TransactionScheduler scheduler(2, data_table.get(), &txn_manager);
    scheduler.Txn(0).Insert(10, 10);
    scheduler.Txn(1).Insert(10, 11);
    scheduler.Txn(0).Commit();
    scheduler.Txn(1).Commit();

    scheduler.Run();

    EXPECT_TRUE((ResultType::SUCCESS == scheduler.schedules[0].txn_result &&
                 ResultType::ABORTED == scheduler.schedules[1].txn_result) ||
                (ResultType::SUCCESS == scheduler.schedules[1].txn_result &&
                 ResultType::ABORTED == scheduler.schedules[0].txn_result));
  }

  {
    std::unique_ptr<storage::DataTable> data_table(
        TransactionTestsUtil::CreatePrimaryKeyUniqueKeyTable());
    // Test2: update a tuple to be a illegal primary key
    // txn1: update (1, 1) -> (1,11) -- success
    // txn0: update (0, 0) -> (0,1) -- fail
    // txn1 commit
    // txn0 commit
    TransactionScheduler scheduler(2, data_table.get(), &txn_manager);
    scheduler.Txn(1).Update(1, 11);
    scheduler.Txn(0).Update(0, 1);
    scheduler.Txn(1).Commit();
    scheduler.Txn(0).Commit();

    scheduler.Run();

    EXPECT_TRUE(ResultType::ABORTED == scheduler.schedules[0].txn_result);
    EXPECT_TRUE(ResultType::SUCCESS == scheduler.schedules[1].txn_result);
  }

  {
    std::unique_ptr<storage::DataTable> data_table(
        TransactionTestsUtil::CreatePrimaryKeyUniqueKeyTable());
    // Test3: update a tuple to be a legal primary key
    // txn1: update (1, 1) -> (1,11) -- success
    // txn1 commit
    // txn0: update (0, 0) -> (0,1) -- success
    // txn0 commit
    TransactionScheduler scheduler(2, data_table.get(), &txn_manager);
    scheduler.Txn(1).Update(1, 11);
    scheduler.Txn(1).Commit();
    scheduler.Txn(0).Update(0, 1);
    scheduler.Txn(0).Commit();

    scheduler.Run();

    EXPECT_TRUE(ResultType::SUCCESS == scheduler.schedules[0].txn_result);
    EXPECT_TRUE(ResultType::SUCCESS == scheduler.schedules[1].txn_result);
  }
}
#endif

*/
}  // End test namespace
}  // End peloton namespace
