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

#ifndef MYSQL_STATEMENT_HPP
#define MYSQL_STATEMENT_HPP

#include "database/statement.hpp"
#include "database/sql.hpp"

#include "object/object_atomizer.hpp"

#include <mysql/mysql.h>

#include <string>
#include <vector>
#include <type_traits>
#include <iostream>

#ifdef WIN32
#define CPP11_TYPE_TRAITS_NS std::tr1
#else
#define CPP11_TYPE_TRAITS_NS std
#endif

namespace oos {

class database;

namespace mysql {

class mysql_database;

class mysql_statement : public statement, public object_writer
{
public:
  explicit mysql_statement(mysql_database &db);
  mysql_statement(mysql_database &db, const sql &s);
  virtual ~mysql_statement();

  virtual result* execute();
  virtual void prepare(const sql &s);
  virtual void reset();
  virtual void bind(object_atomizable *o);
  
  virtual int column_count() const;
  virtual const char* column_name(int i) const;


  virtual database& db();
  virtual const database& db() const;

protected:
  virtual void write(const char *id, char x);
  virtual void write(const char *id, short x);
  virtual void write(const char *id, int x);
  virtual void write(const char *id, long x);
  virtual void write(const char *id, unsigned char x);
  virtual void write(const char *id, unsigned short x);
  virtual void write(const char *id, unsigned int x);
  virtual void write(const char *id, unsigned long x);
  virtual void write(const char *id, float x);
  virtual void write(const char *id, double x);
  virtual void write(const char *id, bool x);
	virtual void write(const char *id, const char *x, int s);
  virtual void write(const char *id, const varchar_base &x);
  virtual void write(const char *id, const std::string &x);
	virtual void write(const char *id, const object_base_ptr &x);
  virtual void write(const char *id, const object_container &x);

  virtual void prepare_result_column(const sql::field_ptr &fptr);

private:
  template < class T >
  void bind_value(MYSQL_BIND &bind, enum_field_types type, T value, int index)
  {
    if (bind.buffer == 0) {
      // allocating memory
//      std::cout << "allocating " << sizeof(T) << " bytes of memory\n";
      bind.buffer = new char[sizeof(T)];
      host_data[index] = true;
    }
    *static_cast<T*>(bind.buffer) = value;
    bind.buffer_type = type;
    bind.is_null = 0;
  }

  static enum_field_types type_enum(sql::data_type_t type);

private:
  mysql_database &db_;
  std::string sqlstr;
  int result_size;
  int result_index;
  int host_size;
  int host_index;
  std::vector<bool> host_data;
  MYSQL_STMT *stmt;
  MYSQL_BIND *result_array;
  MYSQL_BIND *host_array;
};

}

}

#endif /* MYSQL_STATEMENT_HPP */
