#include "libdxfrw_functions.h"

libdxfrw_functions::libdxfrw_functions()
{

}

std::vector<Handle(AIS_Shape)> libdxfrw_functions::open_dxf_file(std::string filename){

    dx_iface().clear(&fData);           // Clear previous loaded dxf data.
    std::vector<Handle(AIS_Shape)> Ais_shapevec;

    bool ok = iface->fileImport( filename, &fData );
    if (!ok) {
        std::cout << "Error reading file " << filename << std::endl;
    } else {
        std::cout << "Ok reading file " << filename << std::endl;

        // Process opencascade primitives.
        // load_opencascade_primitives();

        Ais_shapevec=convert_dxf_into_opencascade_shapes();

    }
    return Ais_shapevec;
}

bool libdxfrw_functions::save_dxf_file(std::string filename){

    bool ok = iface->fileExport(filename, DRW::AC1027 , false, &fData);
    if (!ok) {
        std::cout << "Error saving file " << filename << std::endl;
    } else {
        std::cout << "Ok saving file " << filename << std::endl;
    }
    return ok;
}

std::vector<Handle(AIS_Shape)> libdxfrw_functions::convert_dxf_into_opencascade_shapes(){

    std::cout<<"inside convert dxf function"<<std::endl;
    std::vector<Handle(AIS_Shape)> Ais_shapevec;
    Ais_shapevec.reserve(10000);

    auto blocks = fData.mBlock->ent;
    for (auto iter = blocks.begin(); iter != blocks.end(); iter++){
        if(DRW::POINT == (*iter)->eType){
            std::cout<<"loading point"<<std::endl;
            DRW_Point *point = dynamic_cast<DRW_Point*>(*iter);
            Ais_shapevec.push_back(draw_primitives().draw_3d_point({point->basePoint.x,point->basePoint.y,point->basePoint.z}));
        }
        if(DRW::LINE == (*iter)->eType){
            DRW_Line *line = dynamic_cast<DRW_Line*>(*iter);
            std::cout<<"loading line"<<std::endl;
            // Check if linelenght>0.
            double l=sqrt(pow(line->basePoint.x-line->secPoint.x,2)+pow(line->basePoint.y-line->secPoint.y,2)+pow(line->basePoint.z-line->secPoint.z,2));
            if(l>0){
                Ais_shapevec.push_back(draw_primitives().draw_3d_line({line->basePoint.x,line->basePoint.y,line->basePoint.z},{line->secPoint.x,line->secPoint.y,line->secPoint.z}));
            } else {
                std::cout<<"line is too short to be a line"<<std::endl;
            }
        }
        if(DRW::LWPOLYLINE == (*iter)->eType){
            DRW_LWPolyline *lwpolyline = dynamic_cast<DRW_LWPolyline*>(*iter);
            std::cout<<"loading wire"<<std::endl;
            std::vector<gp_Pnt> pvec;
            for(unsigned int i=0; i<lwpolyline->vertlist.size(); i++){
                pvec.push_back({lwpolyline->vertlist.at(i)->x,lwpolyline->vertlist.at(i)->y,0});
            }
            pvec.push_back({lwpolyline->vertlist.at(0)->x,lwpolyline->vertlist.at(0)->y,0});
            Ais_shapevec.push_back(draw_primitives().draw_3d_line_wire(pvec));
        }
        if(DRW::SPLINE == (*iter)->eType){
            std::cout<<"loading spline"<<std::endl;
            DRW_Spline *spline = dynamic_cast<DRW_Spline*>(*iter);
            std::vector<gp_Pnt> pvec;
            for(unsigned int i=0; i<spline->controllist.size(); i++){
                pvec.push_back({spline->controllist.at(i)->x,spline->controllist.at(i)->y,spline->controllist.at(i)->z});
            }
            Ais_shapevec.push_back(draw_primitives().draw_3d_spline(pvec,5));
        }
        if(DRW::ARC == (*iter)->eType){
            std::cout<<"loading arc"<<std::endl;
            DRW_Arc *arc = dynamic_cast<DRW_Arc*>(*iter);
            Ais_shapevec.push_back(draw_primitives().draw_2d_acad_arc({arc->center().x,arc->center().y,arc->center().z},arc->radius(),arc->startAngle(),arc->endAngle()));
        }
        if(DRW::CIRCLE == (*iter)->eType){
            std::cout<<"loading circle"<<std::endl;
            DRW_Circle *circle = dynamic_cast<DRW_Circle*>(*iter);
            Ais_shapevec.push_back(draw_primitives().draw_2d_circle({circle->basePoint.x,circle->basePoint.y,circle->basePoint.z},circle->radious));

        }
    }
    return Ais_shapevec;
}

bool libdxfrw_functions::write_entity(){

    std::cout<<"add line item"<<std::endl;
    DRW_Line testline;
    testline.basePoint.x = 10;
    testline.basePoint.y = 20;
    testline.secPoint.x = 30;
    testline.secPoint.y = 30;
    iface->addLine(testline);
    return 1;
}
