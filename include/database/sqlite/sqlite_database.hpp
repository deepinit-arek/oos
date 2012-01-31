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

#ifndef SQLITE_DATABASE_HPP
#define SQLITE_DATABASE_HPP

#ifdef WIN32
  #ifdef oos_sqlite_EXPORTS
    #define OOS_SQLITE_API __declspec(dllexport)
  #else
    #define OOS_SQLITE_API __declspec(dllimport)
  #endif
#else
  #define OOS_SQLITE_API
#endif

#include "database/database_impl.hpp"

struct sqlite3;

namespace oos {
  
namespace sqlite {

class sqlite_statement;

/**
 * @class sqlite_database
 * @brief The sqlite database backend
 * 
 * This class is the sqlite database backend
 * class. It provides the sqlite version 3
 */
class OOS_SQLITE_API sqlite_database : public database_impl
{
public:
  sqlite_database();
  virtual ~sqlite_database();
  
  virtual void open(const std::string &db);

  /**
   * Close the database
   */
  virtual void close();

  virtual void visit(create_action *) {}

  /**
   * The interface for the insert action.
   */
  virtual void visit(insert_action *a);

  /**
   * The interface for the update action.
   */
  virtual void visit(update_action *a);

  /**
   * The interface for the delete action.
   */
  virtual void visit(delete_action *a);  

  /**
   * The interface for the drop table action.
   */
  virtual void visit(drop_action *) {}

  /**
   * Create a new sqlite statement
   * 
   * @return A new sqlite statement
   */
  virtual statement_impl* create_statement();

  /**
   * Return the raw pointer to the sqlite3
   * database struct.
   * 
   * @return The raw sqlite3 pointer.
   */
  sqlite3* operator()();

private:
  sqlite3 *db_;
};

}
}

extern "C"
{
  OOS_SQLITE_API oos::database_impl* create_database();

  OOS_SQLITE_API void destroy_database(oos::database_impl *db);
}

#endif /* SQLITE_DATABASE_HPP */