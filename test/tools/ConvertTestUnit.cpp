#include "ConvertTestUnit.hpp"

#include "tools/convert.hpp"
#include "tools/varchar.hpp"

#include <iostream>
#include <string>

using namespace oos;

using std::cout;
using std::string;

ConvertTestUnit::ConvertTestUnit()
  : unit_test("convert test unit")
{
  add_test("convert", std::tr1::bind(&ConvertTestUnit::convert_test, this), "convert test");
}

ConvertTestUnit::~ConvertTestUnit()
{}

void ConvertTestUnit::convert_test()
{
  double dd(0.0);
  
  convert("5.367", dd);
  
  UNIT_ASSERT_EQUAL(dd, 5.367, "unexpected value");

  float f(0.0);
  
  convert("47.11", f);
  
  UNIT_ASSERT_EQUAL(f, 47.11f, "unexpected value");

  int i1(0), i2(3);
  long l1(0);
  short s1(0);
  bool b(true);
  std::string str;
  
  std::string dstr("5.3");
  std::string istr("7");
  std::string b1str("true");
  std::string b2str("0");

  varchar<12> dvar1("18.7");
  varchar<12> dvar2("8.2");

  convert(dvar1, dd);
  convert(dstr, dd);
  convert(istr, i1);
  convert(i2, i1);
  convert(b2str, b);
  convert(i2, l1);
  convert("47.11", dd);
//  convert(l1, s1);
  convert(f, dd);
  convert(b1str, str);
  convert(dvar1, dvar2);
  convert(dstr, l1);
}