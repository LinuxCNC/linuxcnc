#ifndef DRAW_PRIMITIVES_H
#define DRAW_PRIMITIVES_H

#include <OSD.hxx>
#include <AIS_Shape.hxx>
#include <AIS_Trihedron.hxx>
#include <AIS_ViewCube.hxx>
#include <AIS_Selection.hxx>
#include <AIS_ColoredShape.hxx>
#include <AIS_ColoredDrawer.hxx>
#include <AIS_InteractiveContext.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepTools_ReShape.hxx>
#include <BRepTools.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <gp_Trsf.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <TPrsStd_AISPresentation.hxx>
#include <STEPControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Controller.hxx>
#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Geom_CartesianPoint.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GC_MakeArcOfEllipse.hxx>
#include <GC_MakeCircle.hxx>
#include <GC_MakeEllipse.hxx>
#include <GC_MakeSegment.hxx>
#include <gce_MakeRotation.hxx>
#include <TopExp.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Edge.hxx>
#include <TDocStd_Document.hxx>
#include <TDocStd_Application.hxx>
#include <TDF_Label.hxx>
#include <TDF_AttributeIterator.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDataStd_Name.hxx>
#include <TDataStd_TreeNode.hxx>
#include <TDataStd_UAttribute.hxx>
#include <TNaming_NamedShape.hxx>
#include <TopTools.hxx>
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include "Geom_Axis2Placement.hxx"
#include <GCE2d_MakeSegment.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <XCAFDoc_Area.hxx>
#include <XCAFDoc_Centroid.hxx>
#include <XCAFDoc_Datum.hxx>
#include <XCAFDoc_Dimension.hxx>
#include <XCAFDoc_Location.hxx>
#include <XCAFDoc_Volume.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_ColorRGBA.hxx>

#include "XCAFPrs_DocumentExplorer.hxx"
#include <TDataStd_Name.hxx>
#include <XCAFDoc_AssemblyItemId.hxx>
#include <XCAFDoc_AssemblyItemRef.hxx>

#include <Font_BRepFont.hxx>
#include <Font_BRepTextBuilder.hxx>
#include <Bnd_Box.hxx>

#include "gp_Elips.hxx"

#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <Aspect_Handle.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Graphic3d_GraphicDriver.hxx>

#include <NCollection_Mat4.hxx>
#include <gp_Quaternion.hxx>

#include <gce_MakeCirc.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_UniformAbscissa.hxx>

#ifdef Success
#undef Success
#endif

#include <libspline/cubic_spline.h>
using namespace Eigen;

class draw_primitives
{
public:
    draw_primitives();

    // Draw 2d primitives:
    Handle(AIS_Shape) draw_2d_circle(gp_Pnt center,double radius);
    Handle(AIS_Shape) draw_cp_2d_arc(gp_Pnt center, gp_Pnt point1, gp_Pnt point2);
    Handle(AIS_Shape) draw_2d_acad_arc(gp_Pnt center, double radius, const Standard_Real alpha1, const Standard_Real alpha2);
    Handle(AIS_Shape) draw_2d_ellipse(gp_Pnt center, gp_Pnt secpoint, double alpha_start, double alpha_end, double ratio);
    Handle(AIS_Shape) draw_2d_text(std::string str, int textheight, gp_Pnt point, double rot_deg);

    // Draw 3d primitives:
    Handle(AIS_Shape) draw_3d_point(gp_Pnt point);
    Handle(AIS_Shape) draw_3d_line(gp_Pnt point1, gp_Pnt point2);
    Handle(AIS_Shape) draw_3d_line_wire(std::vector<gp_Pnt> points);
    Handle(AIS_Shape) draw_3p_3d_arc(gp_Pnt point1, gp_Pnt point2, gp_Pnt point3);
    Handle(AIS_Shape) draw_cp_3d_arc(gp_Pnt point1, gp_Pnt point2, gp_Pnt center); // Test
    Handle(AIS_Shape) draw_3p_3d_circle(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3);
    Handle(AIS_Shape) draw_3d_spline(std::vector<gp_Pnt> pointvec, int divisions); // Divisions is spline resolution (number of line fragments / segmentation value).

    // Draw 3d solids:
    Handle(AIS_Shape) draw_3d_cone(gp_Pnt centerpoint, double bottomdiameter, double topdiameter, double height);
    Handle(AIS_Shape) draw_3d_tcp_cone(gp_Pnt centerpoint, double bottomdiameter, double topdiameter, double height);
    Handle(AIS_Shape) draw_3d_cilinder(double radius, double height);
    Handle(AIS_Shape) draw_3d_sphere(double radius, gp_Pnt center);
    Handle(AIS_Shape) draw_3d_box(double dx, double dy, double dz);

    // Draw 3d tools:
    std::vector<Handle(AIS_Shape)> draw_3d_arc_lenght(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3);
    double get_3d_arc_lenght(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3, int divisions);
    gp_Pnt get_3d_arc_point_given_arclenght_fromstartpoint(gp_Pnt point1,gp_Pnt point2,gp_Pnt point3, double arclenght_from_startpoint);
    Handle(AIS_Shape) rotate_3d(Handle(AIS_Shape) shape, gp_Pnt center, double euler_z, double euler_y, double euler_x);
    Handle(AIS_Shape) rotate_translate_3d_quaternion(Handle(AIS_Shape) shape, gp_Pnt translation, double euler_z, double euler_y, double euler_x);
    Handle(AIS_Shape) translate_3d(Handle(AIS_Shape) shape, gp_Pnt current, gp_Pnt target);
    Handle(AIS_Shape) colorize(Handle(AIS_Shape) shape, Quantity_Color color, double transparancy);
    gp_Pnt midpoint(gp_Pnt point1, gp_Pnt point2);

    // Draw 3d sets:
    Handle(AIS_Shape) draw_3d_origin(gp_Pnt origin, double linelenght);
    Handle(AIS_Shape) draw_3d_point_origin_cone(gp_Pnt point, gp_Pnt euler);
    Handle(AIS_Shape) draw_3d_point_origin_cone_text(gp_Pnt point, gp_Pnt euler, std::string text, int textheight, int textrotation);
    Handle(AIS_Shape) draw_3d_line_origin_cone_text(gp_Pnt point1, gp_Pnt point2, gp_Pnt euler1, gp_Pnt euler2, std::string text, int textheight);
    Handle(AIS_Shape) draw_3d_wire_origin_cone_text(std::vector<gp_Pnt> points, std::vector<gp_Pnt> euler, std::string text, int textheight);
    Handle(AIS_Shape) draw_3d_arc_origin_cone_text(std::vector<gp_Pnt> points, std::vector<gp_Pnt> euler, std::string text, int textheight);
    Handle(AIS_Shape) draw_3d_circle_origin_cone_text(std::vector<gp_Pnt> points, std::vector<gp_Pnt> euler, std::string text, int textheight);
    Handle(AIS_Shape) draw_3d_spline_origin_cone_text(std::vector<gp_Pnt> points, std::vector<gp_Pnt> euler, int divisions, std::string text, int textheight);


};

#endif // DRAW_PRIMITIVES_H




















