#include "gcode_program.h"

using namespace std;

namespace gpr {

  ostream& operator<<(ostream& stream, const chunk& ic) {
    ic.print(stream);
    return stream;
   }

  ostream& operator<<(ostream& stream, const block& block) {
    block.print(stream);
    return stream;
  }

  ostream& operator<<(ostream& stream, const gcode_program& program) {
    for (auto b : program) { stream << b << endl; }
    return stream;
  }

  addr make_int_address(const int i) {
    addr_value v;
    v.int_val = i;
    return addr{ADDRESS_TYPE_INTEGER, v};
  }

  addr make_double_address(const double i) {
    addr_value v;
    v.dbl_val = i;
    return addr{ADDRESS_TYPE_DOUBLE, v};
  }
  
  chunk make_word_int(const char c, const int i) {
    addr int_address = make_int_address(i);
    return chunk(c, int_address);
  }

  chunk make_word_double(const char c, const double i) {
    addr double_addr = make_double_address(i);
    return chunk(c, double_addr);
  }

  bool operator==(const chunk& l, const chunk& r) {
    return l.equals(r);
  }

  bool operator!=(const chunk& l, const chunk& r) {
    return !(l == r);
  }

  chunk make_comment(const char start_delim,
		     const char end_delim,
		     const std::string& comment_text) {
    return chunk(start_delim, end_delim, comment_text);
  }

  chunk make_percent_chunk() {
    return chunk();
  }

  chunk make_isolated_word(const char c) {
    return chunk(c);
  }
}
