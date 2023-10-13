#include <iostream>
#include <fstream>

#include "catch.hpp"

#include "parser.h"

using namespace std;

namespace gpr {

  TEST_CASE("One line program") {
    gcode_program p = parse_gcode("G0 X1.0 Y1.0");
    REQUIRE(p.num_blocks() == 1);
  }

  TEST_CASE("Two line program") {
    gcode_program p = parse_gcode("G0 X1.0 Y1.0\nG1 X0.0 Y0.0 Z1.2 F12.0");
    REQUIRE(p.num_blocks() == 2);
  }

  TEST_CASE("Correct first token") {
    gcode_program p = parse_gcode("G0 X1.0 Y1.0\nG1 X0.0 Y0.0 Z1.2 F12.0");

    chunk g1 = make_word_int('G', 1);

    SECTION("First token is G1") {
      REQUIRE(p.get_block(1).get_chunk(0) == g1);
    }

    SECTION("Second token is not G1") {
      REQUIRE(p.get_block(0).get_chunk(0) != g1);
    }

    chunk f12 = make_word_double('F', 12.0);

    SECTION("Double addresses") {
      REQUIRE(p.get_block(1).get_chunk(4) == f12);
    }

    SECTION("Double addresses") {
      REQUIRE(p.get_block(1).get_chunk(3) != f12);
    }
    
  }

  TEST_CASE("Different comments with same delimiters are not equal") {
    gcode_program p =
      parse_gcode("( This is a comment )\n M23 ( And so is this ) G54");

    REQUIRE(p.get_block(0).get_chunk(0) != p.get_block(1).get_chunk(1));
  }

  TEST_CASE("Same comments with same delimiters are equal") {
    gcode_program p =
      parse_gcode("( This is a comment G2 )\n M23 ( This is a comment G2 ) G54");

    REQUIRE(p.get_block(0).get_chunk(0) == p.get_block(1).get_chunk(1));
  }

  TEST_CASE("Same comments with different delimiters are not equal") {
    gcode_program p =
      parse_gcode("( This is a comment G2 )\n M23 [ This is a comment G2 ] G54");

    REQUIRE(p.get_block(0).get_chunk(0) != p.get_block(1).get_chunk(1));
  }

  TEST_CASE("Second block is deleted") {
    gcode_program p =
      parse_gcode("( This is a comment G2 )\n /M23 [ This is a comment G2 ] G54");

    REQUIRE(p.get_block(1).is_deleted());
  }

  TEST_CASE("First block is not deleted") {
    gcode_program p =
      parse_gcode("( This is a comment G2 )\n /M23 [ This is a comment G2 ] G54");

    REQUIRE(!(p.get_block(0).is_deleted()));
  }

  TEST_CASE("Lex line") {
  }

  TEST_CASE("3rd block is labeled line 103") {
    gcode_program p =
      parse_gcode("(*** Toolpath 1 ***)\n G0 X0.0 Y0.0 Z0.0 \n N103 G1 X1.0 F23.0\nG1 Z-1.0 F10.0");

    SECTION("Has line number") {
      REQUIRE(p.get_block(2).has_line_number());
    }

    SECTION("Number is 103") {
      REQUIRE(p.get_block(2).line_number() == 103);
    }

    SECTION("4th block does not have a line number") {
      REQUIRE(!(p.get_block(3).has_line_number()));
    }

  }

  TEST_CASE("Parsing Cura style-semicolon comments") {
    gcode_program p =
      parse_gcode(";Generated with Cura_SteamEngine 2.5.0\nM190 S60\nM104 S200\nM109 S200\nG28 ;Home");

    block b = p.get_block(0);
    REQUIRE(b.get_chunk(0).tp() == CHUNK_TYPE_COMMENT);
  }

  TEST_CASE("Parse 3D printer E-block") {
    gcode_program p = parse_gcode(";Prime the extruder\nG92 E0");

    REQUIRE(p.num_blocks() == 2);
  }

  TEST_CASE("Make sure block text gets saved") {

    gcode_program p =
      parse_gcode_saving_block_text("(*** Toolpath 2 ***)\n G0 X1.5 Y0.0 Z0.0 \n N103 G1 X1.0 F23.0\nG1 Z-1.0 F10.0");
    
    REQUIRE(p.num_blocks() == 4);

    string expected_text_for_block_0 = "(*** Toolpath 2 ***) ";
    REQUIRE(p.get_block(0).to_string() == expected_text_for_block_0);
    
    string expected_text_for_block_1 = "G0 X1.5 Y0 Z0 ";
    REQUIRE(p.get_block(1).to_string() == expected_text_for_block_1);

  }

  TEST_CASE("Lex block with isolated P word") {
    string program = "/%G99 G82 R0.1 Z-0.1227 P F15.04";
    vector<string> lexed_line = lex_block(program);

    REQUIRE(lexed_line.size() == 13);

  }

  TEST_CASE("Parse percent sign") {
    gcode_program p =
      parse_gcode_saving_block_text("%");

    REQUIRE(p.get_block(0).get_chunk(0) == make_percent_chunk());
  }

  TEST_CASE("Parsing HAAS block with isolated word 'P'") {
    gcode_program p =
      parse_gcode("G99 G82 R0.1 Z-0.1227 P F15.04");

    REQUIRE(p.get_block(0).get_chunk(4).tp() == CHUNK_TYPE_WORD);
  }

  TEST_CASE("Parse blank line") {
    gcode_program p =
      parse_gcode("G99 G82 R0.1 Z-0.1227 P F15.04\n   ");

    REQUIRE(p.num_blocks() == 2);
  }

  TEST_CASE("Parse CAMASTER style feedrate controls") {
    gcode_program p =
      parse_gcode("F10 XY [SET FEEDRATE FOR X AND Y]");

    REQUIRE(p.num_blocks() == 1);

    REQUIRE(p.get_block(0).size() == 4);
  }
  
  TEST_CASE("Full sample parsing") {

    SECTION("Parse mazak sample") {
      std::ifstream t("./gcode_samples/mazak_sample.EIA");
      std::string file_contents((std::istreambuf_iterator<char>(t)),
    				std::istreambuf_iterator<char>());

      gcode_program p = parse_gcode(file_contents);

      REQUIRE(p.get_block(28).size() == 3);
    }
    
    SECTION("Parse linuxCNC sample") {
      std::ifstream t("./gcode_samples/linuxcnc_sample.ngc");
      std::string file_contents((std::istreambuf_iterator<char>(t)),
    				std::istreambuf_iterator<char>());

      gcode_program p = parse_gcode(file_contents);

      REQUIRE(p.get_block(30341).size() == 5);
    }

    SECTION("Parse CAMASTER sample") {
      std::ifstream t("./gcode_samples/camaster_sample.tap");
      std::string file_contents((std::istreambuf_iterator<char>(t)),
    				std::istreambuf_iterator<char>());

      gcode_program p = parse_gcode(file_contents);

      REQUIRE(p.get_block(42).size() == 4);
      
    }
    
    SECTION("Parse HAAS sample") {
      std::ifstream t("./gcode_samples/HAAS_sample.NCF");
      std::string file_contents((std::istreambuf_iterator<char>(t)),
    				std::istreambuf_iterator<char>());

      gcode_program p = parse_gcode(file_contents);

      REQUIRE(p.get_block(42).size() == 1);
      
    }

    SECTION("Parse Cura sample") {
      std::ifstream t("./gcode_samples/cura_3D_printer.gcode");
      std::string file_contents((std::istreambuf_iterator<char>(t)),
				std::istreambuf_iterator<char>());

      gcode_program p = parse_gcode(file_contents);

      REQUIRE(p.get_block(233).get_chunk(2).get_word() == 'Y');

      REQUIRE(p.get_block(1929).get_chunk(0).get_comment_text() == "TYPE:WALL-INNER");
      
    }

    // SECTION("All files being parsed") {

    //   std::ifstream t1("./gcode_samples/mazak_sample.EIA");
    //   std::string file_contents1((std::istreambuf_iterator<char>(t1)),
    // 				 std::istreambuf_iterator<char>());

    //   cout << file_contents1.size() << endl;

    //   std::ifstream t2("./gcode_samples/linuxcnc_sample.ngc");
    //   std::string file_contents2((std::istreambuf_iterator<char>(t2)),
    // 				 std::istreambuf_iterator<char>());

    //   cout << file_contents2.size() << endl;

    //   std::ifstream t3("./gcode_samples/camaster_sample.tap");
    //   std::string file_contents3((std::istreambuf_iterator<char>(t3)),
    // 				 std::istreambuf_iterator<char>());

    //   cout << file_contents3.size() << endl;
      
    //   std::ifstream t4("./gcode_samples/HAAS_sample.NCF");
    //   std::string file_contents4((std::istreambuf_iterator<char>(t4)),
    // 				 std::istreambuf_iterator<char>());

    //   cout << file_contents4.size() << endl;
 
    //   std::ifstream t5("./gcode_samples/cura_3D_printer.gcode");
    //   std::string file_contents5((std::istreambuf_iterator<char>(t5)),
    // 				 std::istreambuf_iterator<char>());


    //   cout << file_contents5.size() << endl;
    // }
    
  }


  
}
