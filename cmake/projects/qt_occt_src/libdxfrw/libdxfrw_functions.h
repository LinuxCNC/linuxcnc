#ifndef LIBDXFRW_FUNCTIONS_H
#define LIBDXFRW_FUNCTIONS_H

#include <iostream>

// libdxfrw
#include <libdxfrw/dx_data.h>
#include <libdxfrw/dx_iface.h>

// libocct
#include <libocct/opencascade.h>
#include <libocct/draw_primitives.h>

class libdxfrw_functions
{
public:
    libdxfrw_functions();

    std::vector<Handle(AIS_Shape)> open_dxf_file(std::string filename);
    bool save_dxf_file(std::string filename);
    bool write_entity();
    std::vector<opencascade::handle<AIS_Shape>> convert_dxf_into_opencascade_shapes();

private:
    // Dxf data
    dx_data fData;
    dx_data fCleanData; // To clean the fData.
    dx_iface *iface = new dx_iface;
};

#endif // LIBDXFRW_FUNCTIONS_H
