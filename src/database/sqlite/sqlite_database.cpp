/*
 * This file is part of OpenObjectStore OOS.
 *
 * OpenObjectStore OOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenObjectStore OOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenObjectStore OOS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "database/sqlite/sqlite_database.hpp"
#include "database/sqlite/sqlite_statement.hpp"
#include "database/sqlite/sqlite_result.hpp"
#include "database/sqlite/sqlite_types.hpp"
#include "database/sqlite/sqlite_exception.hpp"

#include "database/session.hpp"
#include "database/transaction.hpp"
#include "database/statement_creator.hpp"
#include "database/statement_reader.hpp"
#include "database/statement_binder.hpp"
#include "database/row.hpp"

#include "object/object.hpp"
#include "object/object_store.hpp"
#include "object/prototype_node.hpp"

#include <stdexcept>
#include <sqlite3.h>
#include <iostream>

#ifdef WIN32
#include <functional>
#else
#include <tr1/functional>
#endif

using namespace std::tr1::placeholders;

namespace oos {
  
namespace sqlite {
  
sqlite_database::sqlite_database(session *db)
  : database(db, new sqlite_sequencer(this))
  , sqlite_db_(0)
{
}

sqlite_database::~sqlite_database()
{
  close();
}


void sqlite_database::open(const std::string &db)
{
  if (is_open()) {
    return;
  } else {
    int ret = sqlite3_open(db.c_str(), &sqlite_db_);
    if (ret != SQLITE_OK) {
      throw sqlite_exception("couldn't open database: " + db);
    }
    database::open(db);
  }
}

bool sqlite_database::is_open() const
{
  return sqlite_db_ != 0;
}

void sqlite_database::close()
{
  if (!is_open()) {
    return;
  } else {
    database::close();

    sqlite3_close(sqlite_db_);

    sqlite_db_ = 0;
  }
}

void sqlite_database::create(const prototype_node &node)
{
  create_statement_creator<sqlite_types> creator;

#ifdef WIN32
  std::auto_ptr<object> o(node.producer->create());
#else
  std::unique_ptr<object> o(node.producer->create());
#endif

//  std::cout << "DEBUG: creating table [" << node.type.c_str() << "]\n";
    std::string sql = creator.create(o.get(), node.type.c_str(), "");

    execute(sql.c_str());
}

void sqlite_database::drop(const prototype_node &node)
{
  drop_statement_creator<sqlite_types> creator;

#ifdef WIN32
  std::auto_ptr<object> o(node.producer->create());
#else
  std::unique_ptr<object> o(node.producer->create());
#endif

    std::string sql = creator.create(o.get(), node.type.c_str(), "");

    execute(sql.c_str());
}

void sqlite_database::load(const prototype_node &node)
{
  // create dummy object
#ifdef WIN32
  std::auto_ptr<object> o(node.producer->create());
#else
  std::unique_ptr<object> o(node.producer->create());
#endif
  // try to find select statement statement
  statement_ptr stmt = find_statement(node.type + "_SELECT");
  if (!stmt) {
    select_statement_creator<sqlite_types> creator;
  // state wasn't found, create sql string
    std::string sql = creator.create(o.get(), node.type.c_str(), 0);
  // create the real statement
    stmt.reset(create_statement());
    // prepare statement
    stmt->prepare(sql);
    // store statement
    store_statement(node.type + "_SELECT", stmt);
  }

  table_info_map_t::iterator i = table_info_map().find(node.type);
  if (i == table_info_map().end()) {
    throw sqlite_exception("load: couldn't find node");
  }
//  table_info_t &info = table_info_map().at(node.type);

  statement_reader reader(node, stmt, i->second);
  /* iterate over statement results and create 
   * and insert objects
   */
  reader.import();
  
  // call super class
  database::load(node);
}

void sqlite_database::execute(const char *sql, result_impl *res)
{
  char *errmsg;
  int ret = sqlite3_exec(sqlite_db_, sql, parse_result, res, &errmsg);
  if (ret != SQLITE_OK) {
    std::string error(errmsg);
    sqlite3_free(errmsg);
    throw sqlite_exception(error);
  }
}

void sqlite_database::visit(insert_action *a)
{
  // create insert statement
  statement_ptr stmt = find_statement(std::string(a->type()) + "_INSERT");
  if (!stmt) {
    // create statement
    insert_statement_creator<sqlite_types> creator;

    if (a->empty()) {
      return;
    }
    
    insert_action::const_iterator i = a->begin();
    
    std::string sql = creator.create(*i, a->type().c_str(), "");

    stmt.reset(create_statement());
    stmt->prepare(sql);
    
    store_statement(std::string(a->type()) + "_INSERT", stmt);    
  }
  statement_binder binder;

  insert_action::const_iterator first = a->begin();
  insert_action::const_iterator last = a->end();
  while (first != last) {
    object *o = (*first++);
    binder.bind(stmt, o, false);
    stmt->step();
    stmt->reset(true);
  }
}

void sqlite_database::visit(update_action *a)
{
  statement_ptr stmt = find_statement(std::string(a->obj()->classname()) + "_UPDATE");
  if (!stmt) {
    // create statement
    update_statement_creator<sqlite_types> creator;

    std::string sql = creator.create(a->obj(), a->obj()->classname(), "id=?");

    stmt.reset(create_statement());
    stmt->prepare(sql);
    
    store_statement(std::string(a->obj()->classname()) + "_UPDATE", stmt);    
  }

  statement_binder binder;
  binder.bind(stmt, a->obj(), true);
  stmt->step();
  stmt->reset(true);
}

void sqlite_database::visit(delete_action *a)
{
  statement_ptr stmt = find_statement(std::string(a->classname()) + "_DELETE");
  if (!stmt) {
    // create statement
    delete_statement_creator<sqlite_types> creator;

    std::string sql = creator.create(0, a->classname(), "id=?");

    stmt.reset(create_statement());
    stmt->prepare(sql);
    
    store_statement(std::string(a->classname()) + "_UPDATE", stmt);    
  }

  stmt->bind(1, a->id());
  stmt->step();
  stmt->reset(true);
}

result_impl* sqlite_database::create_result()
{
  return new sqlite_static_result;
}

statement* sqlite_database::create_statement()
{
  return new sqlite_statement(*this);
}

void sqlite_database::prepare_statement(const prototype_node &node,
                         statement *select, statement *insert,
                         statement *update, statement *remove)
{
  // create dummy
  object *o = node.producer->create();
  // prepare select statement
  select_statement_creator<sqlite_types> select_creator;
  // state wasn't found, create sql string
  std::string sql = select_creator.create(o, node.type.c_str(), 0);
  // prepare statement
  select->prepare(sql);
  
  // prepare insert statement
  insert_statement_creator<sqlite_types> insert_creator;
  // state wasn't found, create sql string
  sql = insert_creator.create(o, node.type.c_str(), 0);
  // prepare statement
  insert->prepare(sql);
  
  // prepare insert statement
  update_statement_creator<sqlite_types> update_creator;
  // state wasn't found, create sql string
  sql = update_creator.create(o, node.type.c_str(), "id=?");
  // prepare statement
  update->prepare(sql);
  
  // prepare insert statement
  delete_statement_creator<sqlite_types> delete_creator;
  // state wasn't found, create sql string
  sql = delete_creator.create(o, node.type.c_str(), "id=?");
  // prepare statement
  remove->prepare(sql);
  
  // delete dummy
  delete o;
}

sqlite3* sqlite_database::operator()()
{
  return sqlite_db_;
}

void sqlite_database::on_begin()
{
  execute("BEGIN TRANSACTION;");
}

void sqlite_database::on_commit()
{
  execute("COMMIT TRANSACTION;");
}

void sqlite_database::on_rollback()
{
  execute("ROLLBACK TRANSACTION;");
}

int sqlite_database::parse_result(void* param, int column_count, char** values, char** /*columns*/)
{
  sqlite_static_result *result = static_cast<sqlite_static_result*>(param);

  /********************
   *
   * a new row was retrived
   * add a new row to the result
   * and iterator over all columns
   * adding each column value as
   * string to the row
   *
   ********************/
  std::auto_ptr<row> r(new row);
  for (int i = 0; i < column_count; ++i) {
    r->push_back(std::string(values[i]));
  }
  result->push_back(r.release());
  return 0;
}

}

}

extern "C"
{
  OOS_SQLITE_API oos::database* create_database(oos::session *ses)
  {
    return new oos::sqlite::sqlite_database(ses);
  }

  OOS_SQLITE_API void destroy_database(oos::database *db)
  {
    delete db;
  }
}
