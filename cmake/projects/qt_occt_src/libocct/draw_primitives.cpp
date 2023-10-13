#include "draw_primitives.h"

//! Make conversion's easy:
#define toRadians M_PI/180.0
#define toDegrees (180.0/M_PI)

draw_primitives::draw_primitives()
{

}

// Draw 3d solids
Handle(AIS_Shape) draw_primitives::draw_3d_cone(gp_Pnt centerpoint, double bottomdiameter, double topdiameter, double height){
    gp_Dir axis=gp::DX();
    gp_Ax2 aplace(centerpoint,axis);
    TopoDS_Shape t_topo_cone = BRepPrimAPI_MakeCone(aplace,bottomdiameter,topdiameter,height).Shape();
    return new AIS_Shape(t_topo_cone);
}

Handle(AIS_Shape) draw_primitives::draw_3d_tcp_cone(gp_Pnt centerpoint, double bottomdiameter, double topdiameter, double height){
    gp_Dir axis=-gp::DX();
    gp_Ax2 aplace(centerpoint,axis);
    TopoDS_Shape t_topo_cone = BRepPrimAPI_MakeCone(aplace,bottomdiameter,topdiameter,height).Shape();
    return new AIS_Shape(t_topo_cone);
}

Handle(AIS_Shape) draw_primitives::draw_3d_cilinder(double radius, double height){
    TopoDS_Shape t_topo_cylinder = BRepPrimAPI_MakeCylinder(radius , height).Shape();
    return new AIS_Shape(t_topo_cylinder);
}

Handle(AIS_Shape) draw_primitives::draw_3d_sphere(double radius, gp_Pnt center){
    TopoDS_Shape t_topo_sphere = BRepPrimAPI_MakeSphere(center,radius).Shape();
    return new AIS_Shape(t_topo_sphere);
}

Handle(AIS_Shape) draw_primitives::draw_3d_box(double dx, double dy, double dz){
    // Create workframe box.
    TopoDS_Shape t_topo_box = BRepPrimAPI_MakeBox(dx,dy,dz).Shape();
    Handle(AIS_Shape) t_ais_box = new AIS_Shape(t_topo_box);
    return t_ais_box;
}

// Draw 2d primitives:
Handle(AIS_Shape) draw_primitives::draw_2d_circle(gp_Pnt center,double radius){
    gp_Dir dir(0,0,1);
    gp_Circ circle(gp_Ax2( center, dir),radius);
    BRepBuilderAPI_MakeEdge makeEdge(circle);
    Handle(AIS_Shape) shape = new AIS_Shape(TopoDS_Edge());
    shape ->Set(makeEdge.Edge());
    return shape;
}

Handle(AIS_Shape) draw_primitives::draw_cp_2d_arc(gp_Pnt center, gp_Pnt point1, gp_Pnt point2){

    double radius=center.Distance(point2);
    gp_Dir dir(0,0,1); // you can change this
    gp_Circ circle(gp_Ax2( center, dir),radius);
    Handle(Geom_TrimmedCurve) aArcOfCircle = GC_MakeArcOfCircle(circle,point1,point2,0);
    TopoDS_Edge aEdge2 = BRepBuilderAPI_MakeEdge(aArcOfCircle);
    return new AIS_Shape(aEdge2);
}

Handle(AIS_Shape) draw_primitives::draw_2d_acad_arc(gp_Pnt center, double radius, const Standard_Real alpha1, const Standard_Real alpha2){
    gp_Dir dir(0,0,1); // you can change this
    gp_Circ circle(gp_Ax2( center, dir),radius);
    Handle(Geom_TrimmedCurve) aArcOfCircle = GC_MakeArcOfCircle(circle,alpha1,alpha2,0);
    TopoDS_Edge aEdge = BRepBuilderAPI_MakeEdge(aArcOfCircle);
    return new AIS_Shape(aEdge);
}

Handle(AIS_Shape) draw_primitives::draw_2d_ellipse(gp_Pnt center, gp_Pnt secpoint, double alpha_start, double alpha_end, double ratio){

    //https://github.com/grotius-cnc/QT_CadCam_rev0/blob/master/display/display.h
    // Acad's ellipse nr's, https://github.com/grotius-cnc/cadcam/blob/master/dxf/read_ellipse_AC1027.cpp
    // x,y,z centerpoint    10,20,30
    // x,y,z endpoint mayor 11,21,31 ( coordinates relative to ellipse centerpoint..)
    // ratio                40
    // start angle          41
    // end angle            42

    //Standard_Real alpha1=alpha_start;
    //Standard_Real alpha2=alpha_end;

    //center point to endpoint mayor axis..
    double MayorRadius = sqrt(pow(secpoint.X()-center.X(),2) + pow(secpoint.Y()-center.Y(),2) + pow(secpoint.Z()-center.Z(),2));
    //ratio minor axis to mayor axis..
    double MinorRadius = ratio*MayorRadius ;

    gp_Dir xDirection(secpoint.X()-center.X(),secpoint.Y()-center.Y(),secpoint.Z()-center.Z()); // Direction is auto normalized by occ.
    gp_Dir normalDirection(0,0,1);
    gp_Ax2 axis(center,normalDirection,xDirection);

    gp_Elips ellipse(axis,MayorRadius,MinorRadius);
    Handle(Geom_TrimmedCurve) aArcOfEllipse = GC_MakeArcOfEllipse(ellipse,alpha_start,alpha_end,0);
    TopoDS_Edge aEdge = BRepBuilderAPI_MakeEdge(aArcOfEllipse);

    return new AIS_Shape(aEdge);
}

// Draw 3d tools:
Handle(AIS_Shape) draw_primitives::draw_3d_origin(gp_Pnt origin, double linelenght){

    double x=origin.X();
    double y=origin.Y();
    double z=origin.Z();

    TopoDS_Shape t_topo_sphere = BRepPrimAPI_MakeSphere(origin,2).Shape();
    Handle(AIS_Shape) aisbody_tcp_sphere = new AIS_Shape(t_topo_sphere);
    aisbody_tcp_sphere->SetColor(Quantity_NOC_YELLOW);
    aisbody_tcp_sphere->SetTransparency(0.75);

    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(origin,{x+linelenght,y,z});
    Handle(AIS_Shape) aisbody_tcp_xaxis = new AIS_Shape(edge);
    aisbody_tcp_xaxis->SetColor(Quantity_NOC_RED);

    edge= BRepBuilderAPI_MakeEdge(origin,{x,y+linelenght,z});
    Handle(AIS_Shape) aisbody_tcp_yaxis = new AIS_Shape(edge);
    aisbody_tcp_yaxis->SetColor(Quantity_NOC_GREEN);

    edge= BRepBuilderAPI_MakeEdge(origin,{x,y,z+linelenght});
    Handle(AIS_Shape) aisbody_tcp_zaxis = new AIS_Shape(edge);
    aisbody_tcp_zaxis->SetColor(Quantity_NOC_BLUE);

    aisbody_tcp_sphere->AddChild(aisbody_tcp_xaxis);
    aisbody_tcp_sphere->AddChild(aisbody_tcp_yaxis);
    aisbody_tcp_sphere->AddChild(aisbody_tcp_zaxis);

    return aisbody_tcp_sphere;
}

Handle(AIS_Shape) draw_primitives::rotate_3d(Handle(AIS_Shape) shape, gp_Pnt center, double euler_z, double euler_y, double euler_x){

    // ** For euler angles you have to follow the euler sequence of rotation. First around z, then around y, then around x.
    // Otherwise you will get wrong solutions.

    gp_Trsf trsf1,trsf2,trsf3;
    trsf1.SetRotation(gp_Ax1(center,
                             gp_Dir(0,
                                    0,
                                    1)), euler_z);

    trsf2.SetRotation(gp_Ax1(center,
                             gp_Dir(0,
                                    1,
                                    0)), euler_y);

    trsf3.SetRotation(gp_Ax1(center,
                             gp_Dir(1,
                                    0,
                                    0)), euler_x);

    trsf1.Multiply(trsf2);
    trsf1.Multiply(trsf3);
    shape->SetLocalTransformation(trsf1);
    return shape;
}

Handle(AIS_Shape) draw_primitives::rotate_translate_3d_quaternion(Handle(AIS_Shape) shape, gp_Pnt translation, double euler_z, double euler_y, double euler_x){

    // ** For euler angles you have to follow the euler sequence of rotation. First around z, then around y, then around x.
    // Otherwise you will get wrong solutions.

    gp_Trsf trsf;
    gp_Quaternion aQuat;

    aQuat.SetEulerAngles (gp_YawPitchRoll, euler_z, euler_y, euler_x);
    trsf.SetRotation(aQuat);

    gp_Trsf trsf1;
    trsf1.SetTranslation({0,0,0},translation);

    trsf1.Multiply(trsf);

    shape->SetLocalTransformation(trsf1);
    return shape;
}

Handle(AIS_Shape) draw_primitives::translate_3d(Handle(AIS_Shape) shape, gp_Pnt current, gp_Pnt target){
    gp_Trsf trsf;
    trsf.SetTranslation(current,target);
    shape->SetLocalTransformation(trsf);
    return shape;
}

Handle(AIS_Shape) draw_primitives::colorize(Handle(AIS_Shape) shape, Quantity_Color color, double transparancy){
    shape->SetTransparency(transparancy); // 0.0 - 1.0
    shape->SetColor(color);
    shape->SetMaterial(Graphic3d_NOM_PLASTIC);
    return shape;
}

// Draw 3d primitives:
Handle(AIS_Shape) draw_primitives::draw_3d_point(gp_Pnt point){
    TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(point);
    return new AIS_Shape(vertex);
}

Handle(AIS_Shape) draw_primitives::draw_3d_line(gp_Pnt point1, gp_Pnt point2){

    double l=sqrt(pow(point2.X()-point1.X(),2)+pow(point2.Y()-point1.Y(),2)+pow(point2.Z()-point1.Z(),2));
    if(l==0){ // Will trow error.
        point2.SetX(point2.X()+0.01);
    }
    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(point1,point2);
    return new AIS_Shape(edge);
}

Handle(AIS_Shape) draw_primitives::draw_3d_line_wire(std::vector<gp_Pnt> points){
    BRepBuilderAPI_MakeWire wire;
    for(unsigned int i=0; i<points.size()-1; i++){
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(points.at(i),points.at(i+1));
        wire.Add(edge);
    }
    return new AIS_Shape(wire);
}

Handle(AIS_Shape) draw_primitives::draw_3p_3d_arc(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3){
    Handle(Geom_TrimmedCurve) aArcOfCircle = GC_MakeArcOfCircle(point1,point2,point3);
    TopoDS_Edge aEdge = BRepBuilderAPI_MakeEdge(aArcOfCircle);
    return new AIS_Shape(aEdge);
}

Handle(AIS_Shape) draw_primitives::draw_cp_3d_arc(gp_Pnt point1,gp_Pnt point2,gp_Pnt center){
    // Test
    double radius=center.Distance(point1);
    //gp_Dir dir(0,0,1); // you can change this

    //gp_Dir(const Standard_Real Xv, const Standard_Real Yv, const Standard_Real Zv);
    //gp_Vec vec(point1,point2);
    gp_Dir dir(0,0,1);
    gp_Circ circle(gp_Ax2( center, dir),radius);



    Handle(Geom_TrimmedCurve) aArcOfCircle = GC_MakeArcOfCircle(circle,point1,point2,0);
    TopoDS_Edge aEdge2 = BRepBuilderAPI_MakeEdge(aArcOfCircle);
    return new AIS_Shape(aEdge2);
}

Handle(AIS_Shape) draw_primitives::draw_3p_3d_circle(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3){
    Handle(Geom_Circle) aCircle = GC_MakeCircle(point1,point2,point3);
    TopoDS_Edge aEdge2 = BRepBuilderAPI_MakeEdge(aCircle);
    return new AIS_Shape(aEdge2);
}

Handle(AIS_Shape) draw_primitives::draw_3d_spline(std::vector<gp_Pnt> pointvec, int divisions){

    std::vector<Vector3d> path; // libspline input
    for(unsigned int i=0; i<pointvec.size(); i++){
        path.push_back(Vector3d(pointvec.at(i).X(),
                                pointvec.at(i).Y(),
                                pointvec.at(i).Z()));
    }
    CubicSpline c_spline(path,divisions);
    c_spline.BuildSpline(path);
    std::vector<double> c_pathx(c_spline.GetPositionProfile().size());
    std::vector<double> c_pathy(c_spline.GetPositionProfile().size());
    std::vector<double> c_pathz(c_spline.GetPositionProfile().size());

    // Store wire points(p) into pointvector (pvec)
    gp_Pnt p;
    std::vector<gp_Pnt> pvec;
    // Get profile data for position, speed, acceleration, and curvature
    std::vector<double> ti(c_spline.GetPositionProfile().size());
    for(unsigned int i=0;i<c_pathx.size();++i)
    {
        c_pathx[i] = c_spline.GetPositionProfile()[i].x();
        c_pathy[i] = c_spline.GetPositionProfile()[i].y();
        c_pathz[i] = c_spline.GetPositionProfile()[i].z();

        p= {c_pathx[i],c_pathy[i],c_pathz[i]};
        pvec.push_back(p);
    }

    Handle(AIS_Shape) spline=draw_3d_line_wire(pvec);
    return spline;
}

Handle(AIS_Shape) draw_primitives::draw_2d_text(std::string str, int textheight, gp_Pnt point, double rot_deg){ //https://www.youtube.com/watch?v=31SXQLpdNyE

    const char *chartext=str.c_str();

    if(textheight==0 || textheight<0){textheight=1;}

    Font_BRepFont aBrepFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", textheight);
    Font_BRepTextBuilder aTextBuilder;
    TopoDS_Shape a_text_shape = aTextBuilder.Perform(aBrepFont, NCollection_String(chartext));

    Handle(AIS_Shape) ais_shape_text = new AIS_Shape(a_text_shape);

    gp_Trsf MyTrsf_rot;
    MyTrsf_rot.SetRotation(gp_Ax1(gp_Pnt(
                                      0,
                                      0,
                                      0), gp_Dir(
                                      0,                         //rotation flag x
                                      0,                         //rotation flag y
                                      1)), rot_deg * M_PI /180);
    gp_Trsf MyTrsf_trans;
    MyTrsf_trans.SetTranslation(gp_Pnt(0,0,0),point);
    ais_shape_text->SetLocalTransformation(MyTrsf_trans*MyTrsf_rot);
    return ais_shape_text;
}

Handle(AIS_Shape) draw_primitives::draw_3d_point_origin_cone(gp_Pnt point, gp_Pnt euler){

    Handle(AIS_Shape) Ais_shape=draw_3d_point(point);
    Ais_shape=colorize(Ais_shape,Quantity_NOC_BLUE,0);

    // Draw the origin
    Handle(AIS_Shape) Ais_child=draw_3d_origin({0,0,0},25);
    Ais_child=rotate_translate_3d_quaternion(Ais_child,point,
                                             euler.Z(),
                                             euler.Y(),
                                             euler.X());
    Ais_shape->AddChild(Ais_child);

    // Draw the cone
    Ais_child=draw_3d_tcp_cone({0,0,0},0,5,25);
    Ais_child=rotate_translate_3d_quaternion(Ais_child,point,
                                             euler.Z(),
                                             euler.Y(),
                                             euler.X());

    Ais_child=colorize(Ais_child,Quantity_NOC_RED,0.5);
    Ais_shape->AddChild(Ais_child);

    return Ais_shape;
}

Handle(AIS_Shape) draw_primitives::draw_3d_point_origin_cone_text(gp_Pnt point, gp_Pnt euler, std::string text, int textheight, int textrotation){

    Handle(AIS_Shape) Ais_shape=draw_3d_point_origin_cone(point, euler);
    Ais_shape=colorize(Ais_shape,Quantity_NOC_RED,0);
    Handle(AIS_Shape) Ais_text=draw_2d_text(text, textheight, point, textrotation);
    Ais_text=colorize(Ais_text,Quantity_NOC_BLACK,0.5);
    Ais_shape->AddChild(Ais_text);
    return Ais_shape;
}

Handle(AIS_Shape) draw_primitives::draw_3d_line_origin_cone_text(gp_Pnt point1, gp_Pnt point2, gp_Pnt euler1, gp_Pnt euler2, std::string text, int textheight){

    // Draw the line
    Handle(AIS_Shape) Ais_shape=draw_3d_line(point1,point2);
    Ais_shape=draw_primitives().colorize(Ais_shape,Quantity_NOC_BLUE,0);

    // Draw the first origin
    Handle(AIS_Shape) Ais_child=draw_primitives().draw_3d_origin({0,0,0},25);
    Ais_child=rotate_translate_3d_quaternion(Ais_child,point1,
                                             euler1.Z(),
                                             euler1.Y(),
                                             euler1.X());
    Ais_shape->AddChild(Ais_child);

    // Draw the second origin
    Ais_child=draw_primitives().draw_3d_origin({0,0,0},50);
    Ais_child=rotate_translate_3d_quaternion(Ais_child,point2,
                                             euler2.Z(),
                                             euler2.Y(),
                                             euler2.X());
    Ais_shape->AddChild(Ais_child);

    // Draw the first cone
    Ais_child=draw_primitives().draw_3d_tcp_cone({0,0,0},0,5,25);
    Ais_child=draw_primitives().colorize(Ais_child,Quantity_NOC_GREEN,0.5);
    Ais_child=draw_primitives().rotate_translate_3d_quaternion(Ais_child,point1,
                                                               euler1.Z(),
                                                               euler1.Y(),
                                                               euler1.X());
    Ais_shape->AddChild(Ais_child);

    // Draw the second cone
    Ais_child=draw_primitives().draw_3d_tcp_cone({0,0,0},0,5,25);
    Ais_child=draw_primitives().colorize(Ais_child,Quantity_NOC_BLACK,0.5);
    Ais_child=draw_primitives().rotate_translate_3d_quaternion(Ais_child,point2,
                                                               euler2.Z(),
                                                               euler2.Y(),
                                                               euler2.X());
    Ais_shape->AddChild(Ais_child);

    // Draw the text id.
    Handle(AIS_Shape) Ais_text=draw_2d_text(text, textheight, midpoint(point1,point2), 0);
    Ais_text=colorize(Ais_text,Quantity_NOC_BLACK,0.5);
    Ais_shape->AddChild(Ais_text);

    return Ais_shape;
}

Handle(AIS_Shape) draw_primitives::draw_3d_wire_origin_cone_text(std::vector<gp_Pnt> points, std::vector<gp_Pnt> euler, std::string text, int textheight){
    BRepBuilderAPI_MakeWire wire;
    for(unsigned int i=0; i<points.size()-1; i++){
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(points.at(i),points.at(i+1));
        wire.Add(edge);
    }
    Handle(AIS_Shape) Ais_shape=new AIS_Shape(wire);
    Ais_shape=colorize(Ais_shape,Quantity_NOC_BLUE,0);

    for(unsigned int i=0; i<points.size(); i++){
        // Draw the origin
        Handle(AIS_Shape) Ais_child=draw_3d_origin({0,0,0},25);
        Ais_child=rotate_translate_3d_quaternion(Ais_child,points.at(i),
                                                 euler.at(i).Z(),
                                                 euler.at(i).Y(),
                                                 euler.at(i).X());
        Ais_shape->AddChild(Ais_child);

        // Draw the cone
        Ais_child=draw_3d_tcp_cone({0,0,0},0,5,25);
        Ais_child=rotate_translate_3d_quaternion(Ais_child,points.at(i),
                                                 euler.at(i).Z(),
                                                 euler.at(i).Y(),
                                                 euler.at(i).X());
        if(i==0){
            Ais_child=colorize(Ais_child,Quantity_NOC_GREEN,0.5);
        } else if(i==points.size()-1){
            Ais_child=colorize(Ais_child,Quantity_NOC_BLACK,0.5);
        } else {
            Ais_child=colorize(Ais_child,Quantity_NOC_BLUE,0.5);
        }

        Ais_shape->AddChild(Ais_child);

    }

    // Draw the text id.
    for(unsigned int i=0; i<points.size()-1; i++){
        std::string subidtext=text+"."+std::to_string(i);
        Handle(AIS_Shape) Ais_text=draw_2d_text(subidtext, textheight, midpoint(points.at(i),points.at(i+1)), 0);
        Ais_text=colorize(Ais_text,Quantity_NOC_BLACK,0.5);
        Ais_shape->AddChild(Ais_text);
    }

    return Ais_shape;
}

Handle(AIS_Shape) draw_primitives::draw_3d_arc_origin_cone_text(std::vector<gp_Pnt> points, std::vector<gp_Pnt> euler, std::string text, int textheight){

    Handle(AIS_Shape) Ais_shape=draw_3p_3d_arc(points.at(0),points.at(1),points.at(2));
    Ais_shape=colorize(Ais_shape,Quantity_NOC_BLUE,0);

    for(unsigned int i=0; i<points.size(); i++){
        // Draw the origin
        Handle(AIS_Shape) Ais_child=draw_3d_origin({0,0,0},25);
        Ais_child=rotate_translate_3d_quaternion(Ais_child,points.at(i),
                                                 euler.at(i).Z(),
                                                 euler.at(i).Y(),
                                                 euler.at(i).X());
        Ais_shape->AddChild(Ais_child);

        // Draw the cone
        Ais_child=draw_3d_tcp_cone({0,0,0},0,5,25);
        Ais_child=rotate_translate_3d_quaternion(Ais_child,points.at(i),
                                                 euler.at(i).Z(),
                                                 euler.at(i).Y(),
                                                 euler.at(i).X());
        if(i==0){
            Ais_child=colorize(Ais_child,Quantity_NOC_GREEN,0.5);
        } else if(i==points.size()-1){
            Ais_child=colorize(Ais_child,Quantity_NOC_BLACK,0.5);
        } else {
            Ais_child=colorize(Ais_child,Quantity_NOC_BLUE,0.5);
        }

        Ais_shape->AddChild(Ais_child);
    }




    // Draw the text id.
    Handle(AIS_Shape) Ais_text=draw_2d_text(text, textheight, points.at(1) /*midpoint*/, 0);
    Ais_text=colorize(Ais_text,Quantity_NOC_BLACK,0.5);
    Ais_shape->AddChild(Ais_text);

    //std::vector<Handle(AIS_Shape)> shapevec=draw_primitives::draw_3d_arc_lenght(points.at(0), points.at(1), points.at(2));
    //for(unsigned int i=0; i<shapevec.size(); i++){
    //    Ais_shape->AddChild(shapevec.at(i));
    //}

    return Ais_shape;
}

Handle(AIS_Shape) draw_primitives::draw_3d_circle_origin_cone_text(std::vector<gp_Pnt> points, std::vector<gp_Pnt> euler, std::string text, int textheight){

    Handle(AIS_Shape) Ais_shape=draw_3p_3d_circle(points.at(0),points.at(1),points.at(2));
    Ais_shape=colorize(Ais_shape,Quantity_NOC_BLUE,0);

    for(unsigned int i=0; i<points.size(); i++){
        // Draw the origin
        Handle(AIS_Shape) Ais_child=draw_3d_origin({0,0,0},25);
        Ais_child=rotate_translate_3d_quaternion(Ais_child,points.at(i),
                                                 euler.at(i).Z(),
                                                 euler.at(i).Y(),
                                                 euler.at(i).X());
        Ais_shape->AddChild(Ais_child);

        // Draw the cone
        Ais_child=draw_3d_tcp_cone({0,0,0},0,5,25);
        Ais_child=rotate_translate_3d_quaternion(Ais_child,points.at(i),
                                                 euler.at(i).Z(),
                                                 euler.at(i).Y(),
                                                 euler.at(i).X());
        if(i==0){
            Ais_child=colorize(Ais_child,Quantity_NOC_GREEN,0.5);
        } else if(i==points.size()-1){
            Ais_child=colorize(Ais_child,Quantity_NOC_BLACK,0.5);
        } else {
            Ais_child=colorize(Ais_child,Quantity_NOC_BLUE,0.5);
        }

        Ais_shape->AddChild(Ais_child);
    }

    // Draw the text id.
    Handle(AIS_Shape) Ais_text=draw_2d_text(text, textheight, points.at(1) /*midpoint*/, 0);
    Ais_text=colorize(Ais_text,Quantity_NOC_BLACK,0.5);
    Ais_shape->AddChild(Ais_text);

    return Ais_shape;
}

Handle(AIS_Shape) draw_primitives::draw_3d_spline_origin_cone_text(std::vector<gp_Pnt> points, std::vector<gp_Pnt> euler, int divisions, std::string text, int textheight){

    Handle(AIS_Shape) Ais_shape=draw_3d_spline(points, divisions);
    Ais_shape=colorize(Ais_shape,Quantity_NOC_BLUE,0);

    for(unsigned int i=0; i<points.size(); i++){
        // Draw the origin
        Handle(AIS_Shape) Ais_child=draw_3d_origin({0,0,0},25);
        Ais_child=rotate_translate_3d_quaternion(Ais_child,points.at(i),
                                                 euler.at(i).Z(),
                                                 euler.at(i).Y(),
                                                 euler.at(i).X());
        Ais_shape->AddChild(Ais_child);

        // Draw the cone
        Ais_child=draw_3d_tcp_cone({0,0,0},0,5,25);
        Ais_child=rotate_translate_3d_quaternion(Ais_child,points.at(i),
                                                 euler.at(i).Z(),
                                                 euler.at(i).Y(),
                                                 euler.at(i).X());
        if(i==0){
            Ais_child=colorize(Ais_child,Quantity_NOC_GREEN,0.5);
        } else if(i==points.size()-1){
            Ais_child=colorize(Ais_child,Quantity_NOC_BLACK,0.5);
        } else {
            Ais_child=colorize(Ais_child,Quantity_NOC_BLUE,0.5);
        }

        Ais_shape->AddChild(Ais_child);
    }

    // Draw the text id.
    Handle(AIS_Shape) Ais_text=draw_2d_text(text, textheight, points.at(int(points.size()/2)) /* ~midpoint of vector */, 0);
    Ais_text=colorize(Ais_text,Quantity_NOC_BLACK,0.5);
    Ais_shape->AddChild(Ais_text);

    return Ais_shape;
}

gp_Pnt draw_primitives::midpoint(gp_Pnt point1, gp_Pnt point2){
    gp_Pnt midpoint;
    midpoint.SetX((point1.X()+point2.X())/2);
    midpoint.SetY((point1.Y()+point2.Y())/2);
    midpoint.SetZ((point1.Z()+point2.Z())/2);
    return midpoint;
}

std::vector<Handle(AIS_Shape)> draw_primitives::draw_3d_arc_lenght(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3){

    std::vector<Handle(AIS_Shape)> shapevec;

    Handle(Geom_TrimmedCurve) aArcOfCircle = GC_MakeArcOfCircle(point1,point2,point3);

    TopoDS_Edge aEdge2 = BRepBuilderAPI_MakeEdge(aArcOfCircle);

    GeomAdaptor_Curve acrv(aArcOfCircle);
    TColgp_Array1OfPnt pkte(0, 10);
    GCPnts_UniformAbscissa algo(acrv, 10);
    if(algo.IsDone() && algo.NbPoints() > 0)
    {
        int n = algo.NbPoints();
        std::cout<<"total points:"<<n<<std::endl;
        std::cout<<"abscissa dist:"<<algo.Abscissa()<<std::endl; // Distance between the points. Totalcurvelenght = (points-1)*abscissa dist.

        for(int i=1; i<algo.NbPoints()+1; i++){
            double param = algo.Parameter(i);
            gp_Pnt p = aArcOfCircle->Value(param);
            std::cout<<"Px:"<<p.X()<< " Py:" << p.Y() << " Pz:" << p.Z() <<std::endl;

            Handle(AIS_Shape) Ais_shape=draw_3d_sphere(5,p);
            Ais_shape=colorize(Ais_shape,Quantity_NOC_BLUE,0);
            shapevec.push_back(Ais_shape);
        }

        double curvelenght=0;
        curvelenght=(algo.NbPoints()-1)*algo.Abscissa();
        std::cout<<"curvelenght:"<<curvelenght<<std::endl;
    }
    return shapevec;
}

double draw_primitives::get_3d_arc_lenght(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3, int divisions){

    double curvelenght=0;

    Handle(Geom_TrimmedCurve) aArcOfCircle = GC_MakeArcOfCircle(point1,point2,point3);

    TopoDS_Edge aEdge2 = BRepBuilderAPI_MakeEdge(aArcOfCircle);

    GeomAdaptor_Curve acrv(aArcOfCircle);
    TColgp_Array1OfPnt pkte(0, 10);
    int ammount_of_points=divisions+1;
    GCPnts_UniformAbscissa algo(acrv, ammount_of_points);

    if(algo.IsDone() && algo.NbPoints() > 0)
    {
        int n = algo.NbPoints();
        //std::cout<<"total points:"<<n<<std::endl;
        //std::cout<<"abscissa, distance between the points:"<<algo.Abscissa()<<std::endl; // Distance between the points. Totalcurvelenght = (points-1)*abscissa dist.

        //for(int i=1; i<algo.NbPoints()+1; i++){
            //double param = algo.Parameter(i);
            //std::cout<<"algo parameter at i:"<<param<<std::endl;
            //gp_Pnt p = aArcOfCircle->Value(param);
            //std::cout<<"Px:"<<p.X()<< " Py:" << p.Y() << " Pz:" << p.Z() <<std::endl;
        //}
        curvelenght=(algo.NbPoints()-1)*algo.Abscissa();
    }

    //std::cout<<"curvelenght:"<<curvelenght<<std::endl;
    return curvelenght;
}

gp_Pnt draw_primitives::get_3d_arc_point_given_arclenght_fromstartpoint(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3, double arclenght_from_startpoint){

    gp_Pnt p;

    Handle(Geom_TrimmedCurve) aArcOfCircle = GC_MakeArcOfCircle(point1,point2,point3);

    TopoDS_Edge aEdge2 = BRepBuilderAPI_MakeEdge(aArcOfCircle);

    GeomAdaptor_Curve acrv(aArcOfCircle);

    GCPnts_AbscissaPoint test(acrv,arclenght_from_startpoint,0); // Tollerance
    if(test.IsDone()){
        double param = test.Parameter();
        p = aArcOfCircle->Value(param);
        std::cout<<"Px:"<<p.X()<< " Py:" << p.Y() << " Pz:" << p.Z() <<std::endl;
    };

    return p;
}

// Example one:
/*
Handle(AIS_Shape) shape;
shape = draw_primitives().draw_3d_origin(pointvec.back(),50);
double x,y,z;
cart.M.GetEulerZYX(z,y,x);
shape = draw_primitives().rotate_3d(shape,pointvec.back(),z,y,x);
OpencascadeWidget->show_shape(shape);
*/

/* Example two:
Handle(AIS_Shape) shape;
shape = draw_primitives().draw_3d_origin({0,0,0},50);
double x,y,z;
cart.M.GetEulerZYX(z,y,x);
shape = draw_primitives().rotate_3d_quaternion(shape,pointvec.back(),z,y,x);
OpencascadeWidget->show_shape(shape);
*/































