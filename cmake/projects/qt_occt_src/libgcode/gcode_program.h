#pragma once

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace gpr {

  enum address_type {
    ADDRESS_TYPE_INTEGER = 0,
    ADDRESS_TYPE_DOUBLE
  };

  union addr_value {
    double dbl_val;
    int int_val;
  };

  class addr {
  private:
    address_type ad_type;
    addr_value val;

  public:

    addr(const address_type& p_ad_type,
	 const addr_value& p_val) {
      ad_type = p_ad_type;
      val = p_val;
    }

    address_type tp() const { return ad_type; }

    double double_value() const {
      assert(tp() == ADDRESS_TYPE_DOUBLE);

      return val.dbl_val;
    }

    int int_value() const {
      assert(tp() == ADDRESS_TYPE_INTEGER);

      return val.int_val;
    }

    bool equals(const addr& other) const {
      if (other.tp() != tp()) {
	return false;
      }

      if (other.tp() == ADDRESS_TYPE_DOUBLE) {
	return other.double_value() == double_value();
      }

      return other.int_value() == int_value();
    }

    void print(std::ostream& out) const {
      if (tp() == ADDRESS_TYPE_DOUBLE) {
	out << double_value();
	return;
      }

      out << int_value();

    }

  };

  enum chunk_type {
    CHUNK_TYPE_COMMENT,
    CHUNK_TYPE_WORD_ADDRESS,
    CHUNK_TYPE_PERCENT,
    CHUNK_TYPE_WORD
  };

  struct comment_data {
    char left_delim;
    char right_delim;
    std::string comment_text;

    comment_data() : left_delim('('), right_delim(')'), comment_text("") {}
    comment_data(const char p_left_delim,
		 const char p_right_delim,
		 const std::string& p_comment_text) :
      left_delim(p_left_delim),
      right_delim(p_right_delim),
      comment_text(p_comment_text) {}
  };

  struct word_address_data {
    char wd;
    addr adr;

    word_address_data() :
      wd('\0'),
      adr(ADDRESS_TYPE_INTEGER, {-1}) {}

    word_address_data(const char p_wd,
		      const addr p_adr) :
      wd(p_wd), adr(p_adr) {}
    
  };

  // chunk is the class that represents all data that can appear in a block.
  // A chunk can be either a comment or a word-address pair. The vaue of the
  // field chunk_tp is CHUNK_TYPE_COMMENT if the chunk is a comment and is
  // CHUNK_TYPE_WORD_ADDRESS if the chunk is a word-address pair
  // For example in the G-code program below:
  //      (*** Toolpath 1 ***)
  //      G0 X0.0 Y0.0 Z0.0
  //      G1 X1.0 F23.0
  //      G1 Z-1.0 F10.0
  // The program consits of 4 blocks (block is just G-code speak for line).
  // The first block contains 1 chunk, which is the comment "(*** Toolpath 1 ***)".
  // The second block contains 4 chunks, each of which is a pair of a word
  // (a character) and an address (a number). The 4 chunks are:
  // G0, X0.0, Y0.0, and Z0.0. In G0 the word is 'G' and the address is '0'.
  // In X0.0 the word is 'X' and the address is '0.0', and so on.
  class chunk {
  private:
    chunk_type chunk_tp;

    // Comment fields;
    comment_data cd;

    // Word-address fields
    word_address_data wad;

    // Isolated word fields
    char single_word;
    
  public:
    chunk() : chunk_tp(CHUNK_TYPE_PERCENT) {}
    virtual ~chunk() {}

    chunk(const char c) : chunk_tp(CHUNK_TYPE_WORD), single_word(c) {}
	      
    chunk(const char p_left_delim,
	  const char p_right_delim,
	  const std::string& p_comment_text) :
      chunk_tp(CHUNK_TYPE_COMMENT),
      cd(p_left_delim, p_right_delim, p_comment_text)
    {}

    chunk(const char p_wd,
	  const addr p_adr) :
      chunk_tp(CHUNK_TYPE_WORD_ADDRESS),
      cd('(', ')', ""),
      wad(p_wd, p_adr)
    {}
    
    chunk_type tp() const { return chunk_tp; }

    char get_left_delim() const {
      assert(tp() == CHUNK_TYPE_COMMENT);
      return cd.left_delim;
    }

    char get_right_delim() const {
      assert(tp() == CHUNK_TYPE_COMMENT);
      return cd.right_delim;
    }

    std::string get_comment_text() const {
      assert(tp() == CHUNK_TYPE_COMMENT);
      return cd.comment_text;
    }


    char get_word() const {
      assert(tp() == CHUNK_TYPE_WORD_ADDRESS);
      return wad.wd;
    }

    addr get_address() const {
      assert(tp() == CHUNK_TYPE_WORD_ADDRESS);
      return wad.adr;
    }

    char get_single_word() const {
      assert(tp() == CHUNK_TYPE_WORD);
      return single_word;
    }
    
    bool equals_word_address(const chunk& other_addr) const {
      assert(other_addr.tp() == CHUNK_TYPE_WORD_ADDRESS);

      return (get_word() == other_addr.get_word()) &&
	(get_address().equals(other_addr.get_address()));
    }
    
    bool equals_comment(const chunk& other_comment) const {
      assert(other_comment.tp() == CHUNK_TYPE_COMMENT);

      return (get_comment_text() == other_comment.get_comment_text()) &&
  	(get_left_delim() == other_comment.get_left_delim()) &&
  	(get_right_delim() == other_comment.get_right_delim());
    }
    
    virtual bool equals(const chunk& other) const {
      if (other.tp() != tp()) {
	return false;
      }

      if (tp() == CHUNK_TYPE_WORD_ADDRESS) {
	return equals_word_address(other);
      } else if (tp() == CHUNK_TYPE_COMMENT) {
	return equals_comment(other);
      } else if (tp() == CHUNK_TYPE_PERCENT) {
	// Any 2 percent chunks are always equal
	return true;
      } else {
	assert(false);
      }

    }

    void print_comment(std::ostream& stream) const {
      stream << get_left_delim() << get_comment_text() << get_right_delim();
    }

    void print_word_address(std::ostream& stream) const {
      stream << wad.wd;
      wad.adr.print(stream);
    }

    void print_word(std::ostream& stream) const {
      stream << get_single_word();
    }
    
    void print(std::ostream& stream) const {
      if (tp() == CHUNK_TYPE_COMMENT) {
	print_comment(stream);
      } else if (tp() == CHUNK_TYPE_WORD_ADDRESS) {
	print_word_address(stream);
      } else if (tp() == CHUNK_TYPE_PERCENT) {
	stream << "%";
      } else if (tp() == CHUNK_TYPE_WORD) {
	print_word(stream);
      } else {
	assert(false);
      }
    }

  };

  chunk make_comment(const char start_delim,
		     const char end_delim,
		     const std::string& comment_text);

  chunk make_isolated_word(const char c);
  chunk make_word_int(const char c, const int i);
  chunk make_word_double(const char c, const double i);
  chunk make_percent_chunk();

  bool operator==(const chunk& l, const chunk& r);
  bool operator!=(const chunk& l, const chunk& r);

  std::ostream& operator<<(std::ostream& stream, const chunk& ic);

  // A block is really just a line of code, so for example the following program:
  //      (*** Toolpath 1 ***)
  //      G0 X0.0 Y0.0 Z0.0
  //      G1 X1.0 F23.0
  //      G1 Z-1.0 F10.0
  // consists of 4 blocks
  class block {
  protected:
    bool has_line_no;
    int line_no;
    bool slashed_out;
    std::vector<chunk> chunks;


    // Used to make viewing more convenient during debugging
    std::string debug_text;

  public:
    block(const int p_line_no,
	  const bool p_slashed_out,
	  const std::vector<chunk> p_chunks) :
      has_line_no(true),
      line_no(p_line_no),
      slashed_out(p_slashed_out),
      chunks(p_chunks) {
    }

    block(const bool p_slashed_out,
	  const std::vector<chunk> p_chunks) :
      has_line_no(false),
      line_no(-1),
      slashed_out(p_slashed_out),
      chunks(p_chunks) {
    }

    block(const block& other) :
      has_line_no(other.has_line_no),
      line_no(other.line_no),
      slashed_out(other.slashed_out) {

      for (size_t i = 0; i < other.chunks.size(); i++) {
	chunks.push_back( other.chunks[i] );
      }

    }
    
    block& operator=(const block& other) {
      has_line_no = other.has_line_no;
      line_no = other.line_no;
      slashed_out = other.slashed_out;
      for (size_t i = 0; i < other.chunks.size(); i++) {
	chunks.push_back( other.chunks[i] );
      }

      return *this;
    }

    std::string to_string() const {
      std::ostringstream ss;
      this->print(ss);
      return ss.str();
    }

    // Call this function on a block to set the variable debug_text.
    // This is useful when you want
    // to view information about the block in the debugger
    void set_debug_text(const std::string& text) {
      debug_text = text;
    }

    // Default version of set_debug_text that sets the debug text string
    // to a string representation of the block
    void set_debug_text() {
      set_debug_text(this->to_string());
    }
    
    void print(std::ostream& stream) const {
      if (has_line_number()) {
	stream << "N" << line_number() << " ";
      }
      for (auto i : *this) { stream << i << " "; }
    }

    int size() const { return chunks.size(); }

    const chunk& get_chunk(const int i) {
      assert(i < size());

      return chunks[i];
    }

    bool is_deleted() const { return slashed_out; }

    bool has_line_number() const {
      return has_line_no;
    }

    int line_number() const {
      assert(has_line_number());
      return line_no;
    }

    std::vector<chunk>::const_iterator begin() const { return std::begin(chunks); }
    std::vector<chunk>::const_iterator end() const { return std::end(chunks); }

    std::vector<chunk>::iterator begin() { return std::begin(chunks); }
    std::vector<chunk>::iterator end() { return std::end(chunks); }
    
  };

  class gcode_program {
  protected:
    std::vector<block> blocks;

  public:
    gcode_program(const std::vector<block>& p_blocks) :
      blocks(p_blocks) {}

    int num_blocks() const { return blocks.size(); }

    block get_block(const size_t i) {
      assert(i < blocks.size());
      return blocks[i];
    }

    std::vector<block>::const_iterator begin() const { return std::begin(blocks); }
    std::vector<block>::const_iterator end() const { return std::end(blocks); }

    std::vector<block>::iterator begin() { return std::begin(blocks); }
    std::vector<block>::iterator end() { return std::end(blocks); }

  };

  std::ostream& operator<<(std::ostream& stream, const block& block);

  std::ostream& operator<<(std::ostream& stream, const gcode_program& program);

  addr make_int_address(const int i);

  addr make_double_address(const double i);
  
}
