// Copyright (C) 2007 Klaas Gadeyne <first dot last at gmail dot com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

/* Modified from orocos cppunit test code, also published under GPLV2
    begin                : Mon January 10 2005
    copyright            : (C) 2005 Peter Soetens
    email                : peter.soetens@mech.kuleuven.ac.be
*/

#include <cppunit/XmlOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <iostream>
#include <fstream>

int main(int argc, char** argv)
{
    // Get the top level suite from the registry
    CppUnit::Test *suite = CppUnit::TestFactoryRegistry::getRegistry().makeTest();

    // Adds the test to the list of test to run
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( suite );
#ifndef TESTNAME
    std::ofstream outputFile(std::string(suite->getName()+"-result.xml").c_str());
#else
    std::ofstream outputFile((std::string(TESTNAME)+std::string("-result.xml")).c_str());
#endif
    // Change the default outputter to a compiler error format outputter
    runner.setOutputter( new CppUnit::XmlOutputter( &runner.result(),outputFile ) );
    
    // Run the tests.
    bool wasSucessful = runner.run();

    outputFile.close();
    // Return error code 1 if the one of test failed.
    return wasSucessful ? 0 : 1;
}
