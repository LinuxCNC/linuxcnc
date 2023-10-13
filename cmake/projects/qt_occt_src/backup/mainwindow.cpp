#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "cplusplus.h"
#include "libgcode/interface.h"
#include <libdialog/portable-file-dialogs.h>

bool init_model=0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    OpencascadeWidget = new Opencascade(this);
    ui->gridLayout_opencascade->addWidget(OpencascadeWidget);

    //! Set up hal component.
    myHal->init();

    //! This activates a screen update when robot is moving and screen needs to be updated automaticly.
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::Update_Opencascade);
    timer->start(200);
}

MainWindow::~MainWindow()
{
    myHal->close();
    delete ui;
}

void MainWindow::Update_Opencascade()
{    
    //! Feedback from hal.
    ui->lineEdit_j0->setText(QString::number(*myHal->j0_Fb->Pin,'f',3));
    ui->lineEdit_j1->setText(QString::number(*myHal->j1_Fb->Pin,'f',3));
    ui->lineEdit_j2->setText(QString::number(*myHal->j2_Fb->Pin,'f',3));
    ui->lineEdit_j3->setText(QString::number(*myHal->j3_Fb->Pin,'f',3));
    ui->lineEdit_j4->setText(QString::number(*myHal->j4_Fb->Pin,'f',3));
    ui->lineEdit_j5->setText(QString::number(*myHal->j5_Fb->Pin,'f',3));
    ui->lineEdit_cartx->setText(QString::number(*myHal->CartX_Fb->Pin,'f',3));
    ui->lineEdit_carty->setText(QString::number(*myHal->CartY_Fb->Pin,'f',3));
    ui->lineEdit_cartz->setText(QString::number(*myHal->CartZ_Fb->Pin,'f',3));
    ui->lineEdit_eulerx->setText(QString::number(*myHal->EulerX_Fb->Pin,'f',3));
    ui->lineEdit_eulery->setText(QString::number(*myHal->EulerY_Fb->Pin,'f',3));
    ui->lineEdit_eulerz->setText(QString::number(*myHal->EulerZ_Fb->Pin,'f',3));

    ui->lineEdit_gcode_x->setText(QString::number(0,'f',3));
    ui->lineEdit_gcode_y->setText(QString::number(0,'f',3));
    ui->lineEdit_gcode_z->setText(QString::number(0,'f',3));
    ui->lineEdit_gcode_euler_x->setText(QString::number(0,'f',3));
    ui->lineEdit_gcode_euler_y->setText(QString::number(0,'f',3));
    ui->lineEdit_gcode_euler_z->setText(QString::number(0,'f',3));

    // Load machine model stepfiles:
    if(!init_model){

        std::string basepath="/opt/hal-core-2.0/src/hal/components/opencascade/machines/mitsubishi_rv_6sdl/";
        OpencascadeWidget->Readstepfile(basepath+"base.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_1.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_2.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_3.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_4.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_5.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_6.step");

        OpencascadeWidget->setup_tcp_origin();

        init_model=1;
    }

    // Update machine model joints following the hal input pins:
    if(init_model){
        OpencascadeWidget->update_jointpos(*myHal->j0_Fb->Pin*toRadians,
                                           *myHal->j1_Fb->Pin*toRadians,
                                           *myHal->j2_Fb->Pin*toRadians,
                                           *myHal->j3_Fb->Pin*toRadians,
                                           *myHal->j4_Fb->Pin*toRadians,
                                           *myHal->j5_Fb->Pin*toRadians,
                                           myHal->j0_x->Pin,
                                           myHal->j0_y->Pin,
                                           myHal->j0_z->Pin,
                                           myHal->j1_x->Pin,
                                           myHal->j1_y->Pin,
                                           myHal->j1_z->Pin,
                                           myHal->j2_x->Pin,
                                           myHal->j2_y->Pin,
                                           myHal->j2_z->Pin,
                                           myHal->j3_x->Pin,
                                           myHal->j3_y->Pin,
                                           myHal->j3_z->Pin,
                                           myHal->j4_x->Pin,
                                           myHal->j4_y->Pin,
                                           myHal->j4_z->Pin);
    }


}

void MainWindow::gcode(){

    if (!pfd::settings::available()){
        std::cout << "Portable File Dialogs are not available on this platform. \n"
                     "On linux install zenity, $ sudo apt-get install zenity\n";
    }
    auto f = pfd::open_file("Choose files to read", "/home/user/Desktop/Cam/build-qt-dxf-Desktop-Debug/" /*directory().currentdir()*/,
                            { "Dxf Files (.ngc)", "*.ngc",
                              "All Files", "*" }, pfd::opt::none); // Or ::multiselect.
    // functionname(f.result().at(0)); // This lib can open multiple results.

    // (Intro)
    // G21 (unit mm)
    // G40 (cutter compensation off)
    // G80 (cancel canned cycle modal motion)
    // G90 (absolute distance, no offsets)
    // G64P0.01 (path following accuracy)

    // M30 (outtro)

    std::vector<interface::block> blkvec=interface().read_gcode_file(f.result().at(0));

    for(unsigned int i=0; i<f.result().at(0).size(); i++){
        gcode_filename[i]= f.result().at(0).at(i);
    }

    gp_Pnt p1{0,0,0},p2{0,0,0},pc{0,0,0};

    // Robot offsets
    // Kuka x=630, y=0, z=890
    double x=databucket.gcode_x;
    double y=databucket.gcode_x;
    double z=databucket.gcode_x;
    double euler_x=databucket.gcode_euler_x;
    double euler_y=databucket.gcode_euler_y;
    double euler_z=databucket.gcode_euler_z;

    for(unsigned int i=0; i<blkvec.size(); i++){

        p2={blkvec.at(i).X,blkvec.at(i).Y,blkvec.at(i).Z};
        double I=blkvec.at(i).I;
        double J=blkvec.at(i).J;
        double K=blkvec.at(i).K;
        pc.SetX(p1.X()+I);
        pc.SetY(p1.Y()+J);
        pc.SetZ(p1.Z()+K);

        if(i>0 && blkvec.at(i).type=="G0"){
            Handle(AIS_Shape) Ais_shape=draw_primitives().draw_3d_line(p1,p2);
            Ais_shape=draw_primitives().colorize(Ais_shape,Quantity_NOC_BLUE,0);

            Ais_shape=draw_primitives().rotate_translate_3d_quaternion(Ais_shape,{x,y,z},euler_z,euler_y,euler_x);

            //Ais_shape=draw_primitives().translate_3d(Ais_shape,{0,0,0},{x,y,z});
            OpencascadeWidget->show_shape(Ais_shape);
        }
        if(i>0 && blkvec.at(i).type=="G1"){
            Handle(AIS_Shape) Ais_shape=draw_primitives().draw_3d_line(p1,p2);

            Ais_shape=draw_primitives().rotate_translate_3d_quaternion(Ais_shape,{x,y,z},euler_z,euler_y,euler_x);
            // Ais_shape=draw_primitives().translate_3d(Ais_shape,{0,0,0},{x,y,z});
            Ais_shape=draw_primitives().colorize(Ais_shape,Quantity_NOC_BLACK,0);
            OpencascadeWidget->show_shape(Ais_shape);
        }
        if(i>0 && (blkvec.at(i).type=="G2")){

            // Arc section
            // X=xend, Y=yend. For arc given a G0 startposition and a XY endposition. http://linuxcnc.org/docs/html/gcode/g-code.html#gcode:g2-g3
            // I=offset xcenter-xstart, J=offset ycenter-ystart, G2=clockwise (cw), G3=counterclockwise (ccw)
            Handle(AIS_Shape) Ais_shape=draw_primitives().draw_cp_2d_arc(pc,p2,p1);

            Ais_shape=draw_primitives().rotate_translate_3d_quaternion(Ais_shape,{x,y,z},euler_z,euler_y,euler_x);
            //Ais_shape=draw_primitives().translate_3d(Ais_shape,{0,0,0},{x,y,z});
            Ais_shape=draw_primitives().colorize(Ais_shape,Quantity_NOC_BLACK,0);
            OpencascadeWidget->show_shape(Ais_shape);
        }
        if(i>0 && (blkvec.at(i).type=="G3")){
            Handle(AIS_Shape) Ais_shape=draw_primitives().draw_cp_2d_arc(pc,p2,p1);

            Ais_shape=draw_primitives().rotate_translate_3d_quaternion(Ais_shape,{x,y,z},euler_z,euler_y,euler_x);
            //Ais_shape=draw_primitives().translate_3d(Ais_shape,{0,0,0},{x,y,z});
            Ais_shape=draw_primitives().colorize(Ais_shape,Quantity_NOC_BLACK,0);
            OpencascadeWidget->show_shape(Ais_shape);
        }
        p1=p2;
    }
}

// Open dxf example :
/*
void MainWindow::open_dxf(){
    std::string filename=ui->lineEdit_dxf_file->text().toStdString();
    std::vector<Handle(AIS_Shape)> shapes=libdxfrw_functions().open_dxf_file(filename);

    for(unsigned int i=0; i<shapes.size(); i++){
        shapes.at(i)=draw_primitives().colorize(shapes.at(i),Quantity_NOC_BLUE,0);

        gp_Trsf trsf;
        trsf.SetTranslation({0,0,0},{500,0,500});

        shapes.at(i)->SetLocalTransformation(trsf);
        OpencascadeWidget->show_shape(shapes.at(i));
    }
}
*/


void MainWindow::on_pushButton_pressed()
{
    gcode();
}

void MainWindow::on_pushButton_2_pressed()
{
    std::cout<<"btn pressed"<<std::endl;
}
