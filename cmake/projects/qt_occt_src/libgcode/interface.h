#ifndef INTERFACE_H
#define INTERFACE_H

#include <iostream>
#include <vector>
#include <fstream>
#include <libgcode/parser.h>

class interface
{
public:

    // https://github.com/dillonhuff/gpr/
    // https://www.cnccookbook.com/cnc-g-code-arc-circle-g02-g03/
    struct block {
        std::string type="G0";
        double X=0,Y=0,Z=0,I=0,J=0,K=0,F=0;
    };

    interface();
    std::vector<block> read_gcode_file(std::string filename);
};

#endif // INTERFACE_H
