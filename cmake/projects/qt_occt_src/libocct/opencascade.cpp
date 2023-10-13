#include "opencascade.h"

#include <OSD.hxx>
#include <AIS_Trihedron.hxx>
#include <AIS_ViewCube.hxx>
#include <AIS_Selection.hxx>
#include <AIS_ColoredShape.hxx>
#include <AIS_ColoredDrawer.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
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

#include <BRepTools.hxx>
#include <Font_BRepFont.hxx>
#include <Font_BRepTextBuilder.hxx>
#include <Bnd_Box.hxx>

#include "gp_Elips.hxx"
#include <NCollection_Mat4.hxx>
#include <gp_Quaternion.hxx>

//! Make conversion's easy:
#define toRadians M_PI/180.0
#define toDegrees (180.0/M_PI)

using namespace occ;

//strore the data from the stepfile.
std::vector<TopoDS_Shape> Shape;
std::vector<Handle(AIS_Shape)> ais_Shape;
std::vector<Quantity_Color> colv;

SEGMENT Segment;
std::vector<SEGMENT> SegmentVec;

Handle(XCAFDoc_ColorTool) aColorTool;
Handle(XCAFDoc_ShapeTool) aShapeTool;

gp_Trsf level0x1x2x3x4x5x6;

TopoDS_Shape shape_toolx,shape_tooly,shape_toolz;
Handle(AIS_Shape) toolx,tooly,toolz;

Opencascade::Opencascade(QWidget *parent) : QGLWidget(parent)
{
    setBackgroundRole( QPalette::NoRole );
    setMouseTracking( true );
}

bool Opencascade::Readstepfile(const std::string& theStepName)
{
    SegmentVec.push_back(Segment);

    STEPCAFControl_Controller::Init();

    Handle(TDocStd_Document) aDoc;
    Handle(XCAFApp_Application) anApp = XCAFApp_Application::GetApplication();
    anApp->NewDocument("MDTV-XCAF", aDoc);

    STEPCAFControl_Reader aStepReader;
    aStepReader.SetColorMode(true);
    aStepReader.SetNameMode(true);
    aStepReader.SetLayerMode(true);
    aStepReader.SetPropsMode(true);

    aStepReader.ReadFile (theStepName.c_str());
    aStepReader.Transfer (aDoc);

    TDF_Label aRootLabel = aDoc->Main();

    aShapeTool = XCAFDoc_DocumentTool::ShapeTool(aRootLabel);
    aColorTool = XCAFDoc_DocumentTool::ColorTool(aRootLabel);

    Visit(aRootLabel);

    m_view->FitAll();

    return 1;
}

void Opencascade::Visit(const TDF_Label& theLabel)
{
    //theLabel.EntryDump(std::cout);
    Quantity_Color aColor;
    TopoDS_Shape aShape;

    Handle(TDataStd_Name) aName;
    if (theLabel.FindAttribute(TDataStd_Name::GetID(), aName))
    {
        //std::cout << "  Name: " << aName->Get() << std::endl;
    }

    bool skip=0;
    if (aShapeTool->IsShape(theLabel))
    {
        //TopoDS_Shape aShape;
        //if a shape can be made from the current label, look for the color of that label:
        if(aShapeTool->GetShape(theLabel, aShape)){
            //std::cout<<"made a shape"<<std::endl;

            if(aColorTool->GetColor(aShape,XCAFDoc_ColorSurf,aColor)){
                skip=0;
            } else { skip=1;}

        }

        if(skip==0){

            SegmentVec.back().Ais_ShapeVec.push_back(new AIS_Shape(aShape));
            //if(aColor.Name()!=Quantity_NOC_YELLOW && aColor.Name()!=Quantity_NOC_RED){
            SegmentVec.back().Ais_ShapeVec.back()->SetColor(aColor);
            //}
            SegmentVec.back().Ais_ShapeVec.back()->SetDisplayMode(AIS_Shaded);
            SegmentVec.back().Ais_ShapeVec.back()->Attributes()->SetFaceBoundaryDraw(true);
            SegmentVec.back().Ais_ShapeVec.back()->Attributes()->SetFaceBoundaryAspect(
                        new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.));
            SegmentVec.back().Ais_ShapeVec.back()->Attributes()->SetIsoOnTriangulation(true);
            m_context->Display(SegmentVec.back().Ais_ShapeVec.back(),Standard_False);

        }
    }

    /*! Repeat the visit function for each childmember. */
    for (TDF_ChildIterator c(theLabel); c.More(); c.Next())
    {
        Visit(c.Value());
    }
}

void Opencascade::setup_tcp_origin(float x, float y, float z){

    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge({x,y,z},{x+100,y,z});
    aisBody_tcp_xaxis = new AIS_Shape(edge);
    //aisBody_tcp_xaxis->SetLocalTransformation(level0x1x2x3x4x5x6);
    m_context->SetColor(aisBody_tcp_xaxis,Quantity_NOC_RED,Standard_False);
    m_context->SetMaterial(aisBody_tcp_xaxis,Graphic3d_NOM_PLASTIC,Standard_False);
    m_context->Display(aisBody_tcp_xaxis,Standard_False);

    edge= BRepBuilderAPI_MakeEdge({x,y,z},{x,y+100,z});
    aisBody_tcp_yaxis = new AIS_Shape(edge);
    //aisBody_tcp_yaxis->SetLocalTransformation(level0x1x2x3x4x5x6);
    m_context->SetColor(aisBody_tcp_yaxis,Quantity_NOC_GREEN,Standard_False);
    m_context->SetMaterial(aisBody_tcp_yaxis,Graphic3d_NOM_PLASTIC,Standard_False);
    m_context->Display(aisBody_tcp_yaxis,Standard_False);

    edge= BRepBuilderAPI_MakeEdge({x,y,z},{x,y,z+100});
    aisBody_tcp_zaxis = new AIS_Shape(edge);
    //aisBody_tcp_zaxis->SetLocalTransformation(level0x1x2x3x4x5x6);
    m_context->SetColor(aisBody_tcp_zaxis,Quantity_NOC_BLUE,Standard_False);
    m_context->SetMaterial(aisBody_tcp_zaxis,Graphic3d_NOM_PLASTIC,Standard_False);
    m_context->Display(aisBody_tcp_zaxis,Standard_False);
}

void Opencascade::update_jointpos(double j0, double j1, double j2, double j3, double j4, double j5,
                                  float j0_x, float j0_y, float j0_z,
                                  float j1_x, float j1_y, float j1_z,
                                  float j2_x, float j2_y, float j2_z,
                                  float j3_x, float j3_y, float j3_z,
                                  float j4_x, float j4_y, float j4_z){


    double jx=0,jy=0,jz=0;

    // Robot joints.
    SegmentVec.at(1).MyTrsf.SetRotation(gp_Ax1(gp_Pnt(
                                                   0,           // Framevector  x
                                                   0,           //              y
                                                   0), gp_Dir(  //              z
                                                                0,           // Rotationflag x
                                                                0,           //              y
                                                                1)), j0);
    SegmentVec.at(2).MyTrsf.SetRotation(gp_Ax1(gp_Pnt(
                                                   jx+=j0_x,
                                                   jy+=j0_y,
                                                   jz+=j0_z), gp_Dir(
                                                   0,
                                                   1,
                                                   0)), j1);

    SegmentVec.at(3).MyTrsf.SetRotation(gp_Ax1(gp_Pnt(
                                                   jx+=j1_x,
                                                   jy+=j1_y,
                                                   jz+=j1_z), gp_Dir(
                                                   0,
                                                   1,
                                                   0)), j2);

    SegmentVec.at(4).MyTrsf.SetRotation(gp_Ax1(gp_Pnt(
                                                   jx+=j2_x,
                                                   jy+=j2_y,
                                                   jz+=j2_z), gp_Dir(
                                                   1,
                                                   0,
                                                   0)), j3);

    SegmentVec.at(5).MyTrsf.SetRotation(gp_Ax1(gp_Pnt(
                                                   jx+=j3_x,
                                                   jy+=j3_y,
                                                   jz+=j3_z), gp_Dir(
                                                   0,
                                                   1,
                                                   0)), j4);

    // Robot mount flange axis 6.
    SegmentVec.at(6).MyTrsf.SetRotation(gp_Ax1(gp_Pnt(
                                                   jx+=j4_x,
                                                   jy+=j4_y,
                                                   jz+=j4_z), gp_Dir(
                                                   1,
                                                   0,
                                                   0)), j5);

    gp_Trsf level0=SegmentVec.at(0).MyTrsf;
    gp_Trsf level1=SegmentVec.at(1).MyTrsf;
    gp_Trsf level2=SegmentVec.at(2).MyTrsf;
    gp_Trsf level3=SegmentVec.at(3).MyTrsf;
    gp_Trsf level4=SegmentVec.at(4).MyTrsf;
    gp_Trsf level5=SegmentVec.at(5).MyTrsf;
    gp_Trsf level6=SegmentVec.at(6).MyTrsf;

    gp_Trsf level0x1;
    level0x1.Multiply(level0);
    level0x1.Multiply(level1);

    gp_Trsf level0x1x2 = level0x1;//*level2;
    level0x1x2.Multiply(level2);

    gp_Trsf level0x1x2x3 = level0x1x2; //*level3;
    level0x1x2x3.Multiply(level3);

    gp_Trsf level0x1x2x3x4 = level0x1x2x3; //*level4;
    level0x1x2x3x4.Multiply(level4);

    gp_Trsf level0x1x2x3x4x5 = level0x1x2x3x4; //*level5;
    level0x1x2x3x4x5.Multiply(level5);

    level0x1x2x3x4x5x6 = level0x1x2x3x4x5; //level6; // extern var to calculate move in tool direction.
    level0x1x2x3x4x5x6.Multiply(level6);

    // Apply multipied transformation, works if each stepfile represents one joint.
    for(unsigned int i=0; i<SegmentVec.at(0).Ais_ShapeVec.size(); i++){
        SegmentVec.at(0).Ais_ShapeVec.at(i)->SetLocalTransformation(level0);
    }

    for(unsigned int i=0; i<SegmentVec.at(1).Ais_ShapeVec.size(); i++){
        SegmentVec.at(1).Ais_ShapeVec.at(i)->SetLocalTransformation(level0x1);
    }

    for(unsigned int i=0; i<SegmentVec.at(2).Ais_ShapeVec.size(); i++){
        SegmentVec.at(2).Ais_ShapeVec.at(i)->SetLocalTransformation(level0x1x2);
    }

    for(unsigned int i=0; i<SegmentVec.at(3).Ais_ShapeVec.size(); i++){
        SegmentVec.at(3).Ais_ShapeVec.at(i)->SetLocalTransformation(level0x1x2x3);
    }

    for(unsigned int i=0; i<SegmentVec.at(4).Ais_ShapeVec.size(); i++){
        SegmentVec.at(4).Ais_ShapeVec.at(i)->SetLocalTransformation(level0x1x2x3x4);
    }

    for(unsigned int i=0; i<SegmentVec.at(5).Ais_ShapeVec.size(); i++){
        SegmentVec.at(5).Ais_ShapeVec.at(i)->SetLocalTransformation(level0x1x2x3x4x5);
    }

    for(unsigned int i=0; i<SegmentVec.at(6).Ais_ShapeVec.size(); i++){
        SegmentVec.at(6).Ais_ShapeVec.at(i)->SetLocalTransformation(level0x1x2x3x4x5x6);
    }

    // Move x-axis tcp origin along with the machine.
    aisBody_tcp_xaxis->SetLocalTransformation(level0x1x2x3x4x5x6);
    aisBody_tcp_yaxis->SetLocalTransformation(level0x1x2x3x4x5x6);
    aisBody_tcp_zaxis->SetLocalTransformation(level0x1x2x3x4x5x6);

    // Print matrix 3 rows, 4 colums
    /*
    for(unsigned int i=1; i<4; i++){
         for(unsigned int j=1; j<5; j++){
            std::cout<<level0x1x2x3x4x5x6.Value(i,j)<<" * ";
        }
        std::cout<<std::endl;
    }
    std::cout<<std::endl; */

    if(!m_context.IsNull()){
        m_context->CurrentViewer()->Redraw();
    }
}

void Opencascade::show_shape(Handle(AIS_Shape) shape){
    m_context->Display(shape,Standard_False);
}

void Opencascade::draw_preview_cone(std::string type, gp_Trsf trsf){

    gp_Pnt point={525+toollenght-coneheight,0,890};
    gp_Dir xDirection(1,0,0); // Direction is auto normalized by occ.
    gp_Dir normalDirection(0,0,1);
    gp_Ax2 aplace(point,normalDirection,xDirection);

    TopoDS_Shape t_topo_cone = BRepPrimAPI_MakeCone(aplace,conebottomdiameter,conetopdiameter,coneheight).Shape();

    // Draw toolpos cone at startpoint
    previewbucketvec.push_back(new AIS_Shape(t_topo_cone));

    gp_Trsf MyTrsf_Local_Rot;
    MyTrsf_Local_Rot.SetRotation(gp_Ax1(gp_Pnt(
                                            525+toollenght-coneheight,
                                            0,
                                            890), gp_Dir(
                                            0,                         //rotation flag x
                                            1,                         //rotation flag y
                                            0)), 90 * toRadians);

    previewbucketvec.back()->SetLocalTransformation(trsf*MyTrsf_Local_Rot);

    if(type=="startpoint"){
        m_context->SetColor(previewbucketvec.back(),Quantity_NOC_GREEN,Standard_False);
    }
    if(type=="io"){
        m_context->SetColor(previewbucketvec.back(),Quantity_NOC_RED,Standard_False);
    }
    if(type=="waypoint"){
        m_context->SetColor(previewbucketvec.back(),Quantity_NOC_BLUE,Standard_False);
    }
    if(type=="endpoint"){
        m_context->SetColor(previewbucketvec.back(),Quantity_NOC_BLACK,Standard_False);
    }

    m_context->SetTransparency(previewbucketvec.back(),0.5,false);
    m_context->SetMaterial(previewbucketvec.back(),Graphic3d_NOM_PLASTIC,Standard_False);
    m_context->Display(previewbucketvec.back(),Standard_False);
}

void Opencascade::Redraw(){
    //m_context->CurrentViewer()->Redraw();
    m_view->Redraw();
}

void Opencascade::m_initialize_context()
{
    if (m_context.IsNull())
    {

        Handle(Aspect_DisplayConnection) m_display_donnection = new Aspect_DisplayConnection();

        if (m_graphic_driver.IsNull())
        {
            m_graphic_driver = new OpenGl_GraphicDriver(m_display_donnection);
        }

        //! WId window_handle = (WId) winId();
#ifdef _WIN32
        Handle(WNT_Window) wind = new WNT_Window((Aspect_Handle) window_handle);
#else
        Handle(Xw_Window) wind = new Xw_Window(m_display_donnection, winId());
#endif
        m_viewer = new V3d_Viewer(m_graphic_driver);

        m_view = m_viewer->CreateView();
        m_view->SetWindow(wind);

        /*! Set the camera in perspective mode */
        m_view->Camera()->SetProjectionType (Graphic3d_Camera::Projection_Perspective);

        if (!wind->IsMapped())
        {
            wind->Map();
        }
        m_context = new AIS_InteractiveContext(m_viewer);

        m_viewer->SetDefaultLights();
        m_viewer->SetLightOn();

        /// This is the interactivve 3d box
        //  View : top, bottom, side, 3d, etc.

        opencascade::handle<AIS_ViewCube> aisViewCube = new AIS_ViewCube;
        aisViewCube->SetBoxColor(Quantity_NOC_GRAY75);
        //aisViewCube->SetFixedAnimationLoop(false);
        aisViewCube->SetDrawAxes(false);
        aisViewCube->SetSize(55);
        aisViewCube->SetFontHeight(12);
        aisViewCube->SetTransformPersistence(
                    new Graphic3d_TransformPers(
                        Graphic3d_TMF_TriedronPers,
                        Aspect_TOTP_LEFT_UPPER,
                        Graphic3d_Vec2i(85, 85)));
        m_context->Display(aisViewCube, false);

        //aisViewCube->Attributes()->DatumAspect()->LineAspect(Prs3d_DP_XAxis)->SetColor(Quantity_NOC_RED2);
        //const Handle_Prs3d_DatumAspect& datumAspect = aisViewCube->Attributes()->DatumAspect();
        //datumAspect->ShadingAspect(Prs3d_DP_XAxis)->SetColor(Quantity_NOC_RED2);
        //datumAspect->ShadingAspect(Prs3d_DP_YAxis)->SetColor(Quantity_NOC_GREEN2);
        //datumAspect->ShadingAspect(Prs3d_DP_ZAxis)->SetColor(Quantity_NOC_BLUE2);
        //m_aisViewCube = aisViewCube;

        /// Set background homogenius, one color.
        //  m_view->SetBackgroundColor(Quantity_NOC_GRAY49);
        //  m_viewer->SetDefaultShadingModel(Graphic3d_TypeOfShadingModel::V3d_COLOR);

        /// Set background with gradient stylesheet
        //  Gradient sheme's for SetBgGradientColors:
        //  Aspect_GFM_CORNER1
        //  Aspect_GFM_CORNER2
        //  Aspect_GFM_CORNER3
        //  Aspect_GFM_CORNER4
        //  Aspect_GFM_DIAG1
        //  Aspect_GFM_DIAG2
        //  Aspect_GFM_HOR
        //  Aspect_GFM_NONE
        //  Aspect_GFM_VER

        Quantity_Color cola,colb;
        cola.SetValues(0.1,0.1,0.1,Quantity_TOC_RGB);
        colb.SetValues(0.0,0.0,0.0,Quantity_TOC_RGB);
        m_view->SetBgGradientColors(cola,colb,Aspect_GFM_DIAG2 , false);

        //! std::cout<<get_current_dir_name()<<std::endl;
        //! m_view->SetBackgroundImage(theImage,Aspect_FM_CENTERED,Standard_False);

        /// View as wireframe or shaded
        //m_context->SetDisplayMode(AIS_WireFrame, Standard_False);
        m_context->SetDisplayMode(AIS_Shaded, Standard_False);

        /*
        Handle(Prs3d_Drawer) t_hilight_style = m_context->HighlightStyle();
        t_hilight_style->SetMethod(Aspect_TOHM_COLOR);
        t_hilight_style->SetColor(Quantity_NOC_LIGHTYELLOW);
        t_hilight_style->SetDisplayMode(1);
        t_hilight_style->SetTransparency(0.2f);

        Handle(Prs3d_Drawer) t_select_style = m_context->SelectionStyle();
        t_select_style->SetMethod(Aspect_TOHM_COLOR);
        t_select_style->SetColor(Quantity_NOC_LIGHTSEAGREEN);
        t_select_style->SetDisplayMode(1);
        t_select_style->SetTransparency(0.4f);
        */

        /// Show grid
        m_viewer->SetRectangularGridValues(0,0,1,1,0);
        m_viewer->SetRectangularGridGraphicValues(2.01,2.01,0);
        m_viewer->ActivateGrid(Aspect_GT_Rectangular,Aspect_GDM_Lines);

        /// Show triedron. This is the 3d axis cross at the lower left of the screen.
        m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD, 0.08, V3d_ZBUFFER);

        /*
        /// Show triedron at a specific place on the screen
        axis = new Geom_Axis2Placement(gp::XOY());
        aisTrihedron = new AIS_Trihedron(axis);
        aisTrihedron->SetDatumDisplayMode(Prs3d_DM_WireFrame);
        aisTrihedron->SetDrawArrows(true);
        aisTrihedron->Attributes()->DatumAspect()->LineAspect(Prs3d_DP_XAxis)->SetWidth(2.5);
        aisTrihedron->Attributes()->DatumAspect()->LineAspect(Prs3d_DP_YAxis)->SetWidth(2.5);
        aisTrihedron->Attributes()->DatumAspect()->LineAspect(Prs3d_DP_ZAxis)->SetWidth(2.5);
        aisTrihedron->SetDatumPartColor(Prs3d_DP_XAxis, Quantity_NOC_RED2);
        aisTrihedron->SetDatumPartColor(Prs3d_DP_YAxis, Quantity_NOC_GREEN2);
        aisTrihedron->SetDatumPartColor(Prs3d_DP_ZAxis, Quantity_NOC_BLUE2);
        aisTrihedron->SetLabel(Prs3d_DP_XAxis, "");
        aisTrihedron->SetLabel(Prs3d_DP_YAxis, "");
        aisTrihedron->SetLabel(Prs3d_DP_ZAxis, "");
        //aisTrihedron->SetTextColor(Quantity_NOC_GRAY40);
        aisTrihedron->SetSize(60);
        aisTrihedron->SetTransformPersistence(
                    new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, axis->Ax2().Location()));
        aisTrihedron->Attributes()->SetZLayer(Graphic3d_ZLayerId_Topmost);
        aisTrihedron->SetInfiniteState(true);
        m_context->Display(aisTrihedron,Standard_False);
        */

        m_view->MustBeResized();
    }
}

void Opencascade::Set_orthographic(){
    m_view->Camera()->SetProjectionType (Graphic3d_Camera::Projection_Orthographic);
    m_view->Update();
}

void Opencascade::Set_perspective(){
    m_view->Camera()->SetProjectionType (Graphic3d_Camera::Projection_Perspective);
    m_view->Redraw();
    //m_view->Update();
}

void Opencascade::paintEvent(QPaintEvent *)
{
    if (m_context.IsNull())
    {
        m_initialize_context();
    }
    m_view->Redraw();
}

void Opencascade::resizeEvent(QResizeEvent *)
{
    if( !m_view.IsNull() )
    {
        m_view->MustBeResized();
    }
}

void Opencascade::mousePressEvent(QMouseEvent *event)
{
    if((event->buttons()&Qt::LeftButton) && (event->buttons()&Qt::RightButton))
    {
        m_x_max=event->x();
        m_y_max=event->y();
    }
    else if(event->buttons()&Qt::LeftButton)
    {
        m_context->MoveTo(event->pos().x(),event->pos().y(),m_view,Standard_True);

        AIS_StatusOfPick t_pick_status = AIS_SOP_NothingSelected;
        if(qApp->keyboardModifiers()==Qt::ControlModifier)
        {
            t_pick_status = m_context->ShiftSelect(true);
        }
        else
        {
            t_pick_status = m_context->Select(true);
        }
    }
    else if(event->buttons()&Qt::MidButton)
    {
        m_x_max=event->x();
        m_y_max=event->y();
        m_view->StartRotation(event->x(),event->y());
    }
}

void Opencascade::mouseReleaseEvent(QMouseEvent *event)
{
    m_context->MoveTo(event->pos().x(),event->pos().y(),m_view,Standard_True);
}

void Opencascade::mouseMoveEvent(QMouseEvent *event)
{
    if((event->buttons()&Qt::LeftButton) && (event->buttons()&Qt::RightButton))
    {
        m_view->Pan(event->pos().x()-m_x_max,m_y_max-event->pos().y());
        m_x_max=event->x();
        m_y_max=event->y();
    }
    else if(event->buttons()&Qt::MidButton)
    {
        if(qApp->keyboardModifiers()==Qt::ShiftModifier)
        {
            m_view->Pan(event->pos().x()-m_x_max,m_y_max-event->pos().y());
            m_x_max=event->x();
            m_y_max=event->y();
        }
        else
        {
            m_view->Rotation(event->x(),event->y());
        }
    }
    else
    {
        m_context->MoveTo(event->pos().x(),event->pos().y(),m_view,Standard_True);
    }
}

void Opencascade::wheelEvent(QWheelEvent *event)
{
    m_view->StartZoomAtPoint(event->position().x(),event->position().y());
    m_view->ZoomAtPoint(0, 0, event->angleDelta().y(), 0);
}

void Opencascade::set_view_front()
{
    m_view->SetProj( V3d_Yneg );
}

void Opencascade::set_view_back()
{
    m_view->SetProj( V3d_Ypos );
}

void Opencascade::set_view_top()
{
    m_view->SetProj( V3d_Zpos );
}

void Opencascade::set_view_bottom()
{
    m_view->SetProj( V3d_Zneg );
}

void Opencascade::set_view_left()
{
    m_view->SetProj( V3d_Xneg );
}
void Opencascade::set_view_right()
{
    m_view->SetProj( V3d_Xpos );
}












