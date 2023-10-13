#ifndef OPENCASCADE_H
#define OPENCASCADE_H

#include <QWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QApplication>

#include <AIS_InteractiveContext.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <Aspect_Handle.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Graphic3d_GraphicDriver.hxx>

#include <QGLWidget>
#ifdef _WIN32
#include <WNT_Window.hxx>
#else
#undef None
#include <Xw_Window.hxx>
#endif

#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <AIS_Shape.hxx>
#include <TDF_Label.hxx>

//show xyz axis
#include <Geom_Axis2Placement.hxx>
#include <AIS_Trihedron.hxx>

#include <libocct/draw_primitives.h>

#define gp_Euler gp_Pnt // Used as universal toolset.

struct POINT{
    double x=0,y=0,z=0;
};

struct SEGMENT{
    std::vector<Handle(AIS_Shape)> Ais_ShapeVec={}; ///each stepfile can contain multiple shapes, we need the vector.
    gp_Trsf MyTrsf={};
};
extern SEGMENT Segment;
extern std::vector<SEGMENT> SegmentVec;

extern gp_Trsf level0x1x2x3x4x5x6;

namespace occ {
class Opencascade: public QGLWidget
{
    Q_OBJECT
public:
    explicit Opencascade(QWidget *parent = nullptr);

    struct io {
        std::string halcommand={""};
    };

    struct bucket {
        std::string primitivetype;      // line,wire,arc,circle,spline.
        std::vector<gp_Pnt> pointvec;
        std::vector<gp_Pnt> eulervec;
        Handle(AIS_Shape) Ais_shape;
        std::string info;

        double vo=0,ve=0,velmax=0,accmax=0;
    };
    std::vector<bucket> bucketvec;

    std::vector<Handle(AIS_Shape)> previewbucketvec;

    bool Readstepfile(const std::string& theStepName);
    void Visit(const TDF_Label& theLabel);
    void Init_robot();
    void setup_tcp_origin(float x, float y, float z);

    void show_shape(Handle(AIS_Shape) shape);
    void Redraw();
    //! Values in radians.
    void update_jointpos(double j0, double j1, double j2, double j3, double j4, double j5,
                         float j0_x, float j0_y, float j0_z,
                         float j1_x, float j1_y, float j1_z,
                         float j2_x, float j2_y, float j2_z,
                         float j3_x, float j3_y, float j3_z,
                         float j4_x, float j4_y, float j4_z);

    //! Preview line
    void draw_preview_cone(std::string type, gp_Trsf trsf);

    //! View
    void Set_orthographic();
    void Set_perspective();
    void set_view_front();
    void set_view_back();
    void set_view_left();
    void set_view_right();
    void set_view_top();
    void set_view_bottom();

private:
    void m_initialize_context();
    Handle(AIS_InteractiveContext) m_context;
    Handle(V3d_Viewer) m_viewer;
    Handle(V3d_View) m_view;
    Handle(Graphic3d_GraphicDriver) m_graphic_driver;
    Handle(AIS_InteractiveObject) m_aisViewCube;

    //! Xyz axis sign.
    Handle(Geom_Axis2Placement) axis;
    Handle(AIS_Trihedron) aisTrihedron;
    std::vector<Handle(AIS_Trihedron)> aisTrihedrons;

protected:
    void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

protected:
    enum CurrentAction3d
    {
        CurAction3d_Nothing,
        CurAction3d_DynamicPanning,
        CurAction3d_DynamicZooming,
        CurAction3d_DynamicRotation
    };

private:
    Standard_Integer m_x_max;
    Standard_Integer m_y_max;
    CurrentAction3d m_current_mode;
    //gp_Trsf current_tcp;

    Handle(AIS_Shape) aisBody_tcp_xaxis, aisBody_tcp_yaxis, aisBody_tcp_zaxis;

    // Create the euler lines
    double toollenght=105;
    double linelenght=25;
    double coneheight=25;
    double conetopdiameter=1;
    double conebottomdiameter=5;
    double textheight=25;


    TopoDS_Edge edge_linepreview;
    Handle(AIS_Shape) aisBody_linepreview;

signals:

public slots:
};
}

#endif // OPENCASCADE_H


