#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "libgcode/interface.h"
#include <libdialog/portable-file-dialogs.h>
#include <gp_Quaternion.hxx>

bool init_model=0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    OpencascadeWidget = new Opencascade(this);
    ui->gridLayout_opencascade->addWidget(OpencascadeWidget);

    //! Set up hal component.
    // myHal->init();

    //! This activates a screen update when robot is moving and screen needs to be updated automaticly.
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::update);
    timer->start(20);
}

MainWindow::~MainWindow()
{
    // myHal->close();
    delete ui;
}

void MainWindow::update(){
    update_opencascade();
}

void MainWindow::update_opencascade()
{
    //! Feedback from hal.
    ui->lineEdit_j0->setText(QString::number(d.J0,'f',3));
    ui->lineEdit_j1->setText(QString::number(d.J1,'f',3));
    ui->lineEdit_j2->setText(QString::number(d.J2,'f',3));
    ui->lineEdit_j3->setText(QString::number(d.J3,'f',3));
    ui->lineEdit_j4->setText(QString::number(d.J4,'f',3));
    ui->lineEdit_j5->setText(QString::number(d.J5,'f',3));
    ui->lineEdit_cart_x->setText(QString::number(d.Cartx,'f',3));
    ui->lineEdit_cart_y->setText(QString::number(d.Carty,'f',3));
    ui->lineEdit_cart_z->setText(QString::number(d.Cartz,'f',3));
    ui->lineEdit_euler_x->setText(QString::number(d.Eulerx,'f',3));
    ui->lineEdit_euler_y->setText(QString::number(d.Eulery,'f',3));
    ui->lineEdit_euler_z->setText(QString::number(d.Eulerz,'f',3));

    //! Load machine model stepfiles:
    if(!init_model){

        //! mitsubishi_rv_6sdl
        //! Joint input's in degrees
        //! Joint 0
        d.J0_x=85;
        d.J0_y=0;
        d.J0_z=350;
        d.J0=0*toRadians;
        d.J0_min=-170*toRadians;
        d.J0_max=170*toRadians;

        d.J1_x=0;
        d.J1_y=0;
        d.J1_z=380;
        d.J1=0*toRadians;
        d.J1_min=-100*toRadians;
        d.J1_max=135*toRadians;

        d.J2_x=0;
        d.J2_y=0;
        d.J2_z=100;
        d.J2=0*toRadians;
        d.J2_min=-210*toRadians;
        d.J2_max=66*toRadians;

        d.J3_x=425;
        d.J3_y=0;
        d.J3_z=0;
        d.J3=0*toRadians;
        d.J3_min=-185*toRadians;
        d.J3_max=185*toRadians;

        d.J4_x=85;
        d.J4_y=0;
        d.J4_z=0;
        d.J4=0*toRadians;
        d.J4_min=-120*toRadians;
        d.J4_max=120*toRadians;

        d.J5_x=0;
        d.J5_y=0;
        d.J5_z=0;
        d.J5=0*toRadians;
        d.J5_min=-350*toRadians;
        d.J5_max=350*toRadians;

        d.stepsize_cart=50;
        d.stepsize_euler=10;
        d.stepsize_joint=10;
        d.stepsize_tooldir=10;

        std::string basepath="/opt/hal-core-2.0/src/hal/components/opencascade/machines/mitsubishi_rv_6sdl/";
        OpencascadeWidget->Readstepfile(basepath+"base.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_1.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_2.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_3.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_4.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_5.step");
        OpencascadeWidget->Readstepfile(basepath+"joint_6.step");

        d.stepsize_cart=ui->spinBox_stepsize_cart->value();
        d.stepsize_euler=ui->spinBox_stepsize_euler->value();
        d.stepsize_joint=ui->spinBox_stepsize_joint->value();
        d.stepsize_tooldir=ui->spinBox_stepsize_tooldir->value();
        d.iterations=ui->spinBox_ik_iterations->value();
        d.motion_continious=ui->checkBox_motion_continious->isChecked();
        d.Ik_from_init=ui->checkBox_ik_from_init->isChecked();
        d=myKin->init(d);

        OpencascadeWidget->setup_tcp_origin(d.Cartx,
                                            d.Carty,
                                            d.Cartz);



        init_model=1;
    }

    //! Update machine pos.
    if(init_model){
        OpencascadeWidget->update_jointpos(d.J0,
                                           d.J1,
                                           d.J2,
                                           d.J3,
                                           d.J4,
                                           d.J5,
                                           d.J0_x,
                                           d.J0_y,
                                           d.J0_z,
                                           d.J1_x,
                                           d.J1_y,
                                           d.J1_z,
                                           d.J2_x,
                                           d.J2_y,
                                           d.J2_z,
                                           d.J3_x,
                                           d.J3_y,
                                           d.J3_z,
                                           d.J4_x,
                                           d.J4_y,
                                           d.J4_z);

        checkStatus();
        checkContiniousMotion();
    }
}
//! Display messages.
void MainWindow::checkStatus(){
    //! Display error info.
    if(d.error){
        ui->checkBox_error->setChecked(true);

        //! Solver sequence, try different approach.
        d.Ik_from_init=!d.Ik_from_init;
        //! Update checkbox.
        ui->checkBox_ik_from_init->setChecked(d.Ik_from_init);

    } else {
        ui->checkBox_error->setChecked(false);
    }
    //! Display kinematic mode.
    if(d.mode_fk){
        ui->checkBox_fk->setChecked(true);
        ui->checkBox_ik->setChecked(false);
    } else {
        ui->checkBox_fk->setChecked(false);
        ui->checkBox_ik->setChecked(true);
    }
}
//! The gui timer value threathens this function.
void MainWindow::checkContiniousMotion(){
    if(d.motion_continious){
        if(d.motion_x_plus){
            on_pushButton_x_plus_pressed();
        }
        if(d.motion_x_min){
            on_pushButton_x_min_pressed();
        }
        if(d.motion_y_plus){
            on_pushButton_y_plus_pressed();
        }
        if(d.motion_y_min){
            on_pushButton_y_min_pressed();
        }
        if(d.motion_z_plus){
            on_pushButton_z_plus_pressed();
        }
        if(d.motion_z_min){
            on_pushButton_z_min_pressed();
        }
        if(d.motion_j0_plus){
            on_pushButton_j0_plus_pressed();
        }
        if(d.motion_j0_min){
            on_pushButton_j0_min_pressed();
        }
        if(d.motion_j1_plus){
            on_pushButton_j1_plus_pressed();
        }
        if(d.motion_j1_min){
            on_pushButton_j1_min_pressed();
        }
        if(d.motion_j2_plus){
            on_pushButton_j2_plus_pressed();
        }
        if(d.motion_j2_min){
            on_pushButton_j2_min_pressed();
        }
        if(d.motion_j3_plus){
            on_pushButton_j3_plus_pressed();
        }
        if(d.motion_j3_min){
            on_pushButton_j3_min_pressed();
        }
        if(d.motion_j4_plus){
            on_pushButton_j4_plus_pressed();
        }
        if(d.motion_j4_min){
            on_pushButton_j4_min_pressed();
        }
        if(d.motion_j5_plus){
            on_pushButton_j5_plus_pressed();
        }
        if(d.motion_j5_min){
            on_pushButton_j5_min_pressed();
        }
        if(d.motion_euler_x_plus){
            on_pushButton_euler_x_plus_pressed();
        }
        if(d.motion_euler_x_min){
            on_pushButton_euler_x_min_pressed();
        }
        if(d.motion_euler_y_plus){
            on_pushButton_euler_y_plus_pressed();
        }
        if(d.motion_euler_y_min){
            on_pushButton_euler_y_min_pressed();
        }
        if(d.motion_euler_z_plus){
            on_pushButton_euler_z_plus_pressed();
        }
        if(d.motion_euler_z_min){
            on_pushButton_euler_z_min_pressed();
        }
        if(d.motion_tool_x_plus){
            on_pushButton_tool_x_plus_pressed();
        }
        if(d.motion_tool_x_min){
            on_pushButton_tool_x_min_pressed();
        }
        if(d.motion_tool_y_plus){
            on_pushButton_tool_y_plus_pressed();
        }
        if(d.motion_tool_y_min){
            on_pushButton_tool_y_min_pressed();
        }
        if(d.motion_tool_z_plus){
            on_pushButton_tool_z_plus_pressed();
        }
        if(d.motion_tool_z_min){
            on_pushButton_tool_z_min_pressed();
        }
    }
}

void MainWindow::on_pushButton_x_min_pressed()
{
    d.Cartx-=d.stepsize_cart;
    d=myKin->ik(d);
    d.motion_x_min=1;
}

void MainWindow::on_pushButton_x_plus_pressed()
{
    d.Cartx+=d.stepsize_cart;
    d=myKin->ik(d);
    d.motion_x_plus=1;
}

void MainWindow::on_pushButton_y_min_pressed()
{
    d.Carty-=d.stepsize_cart;
    d=myKin->ik(d);
    d.motion_y_min=1;
}

void MainWindow::on_pushButton_y_plus_pressed()
{
    d.Carty+=d.stepsize_cart;
    d=myKin->ik(d);
    d.motion_y_plus=1;
}

void MainWindow::on_pushButton_z_min_pressed()
{
    d.Cartz-=d.stepsize_cart;
    d=myKin->ik(d);
    d.motion_z_min=1;
}

void MainWindow::on_pushButton_z_plus_pressed()
{
    d.Cartz+=d.stepsize_cart;
    d=myKin->ik(d);
    d.motion_z_plus=1;
}

void MainWindow::on_pushButton_j0_min_pressed()
{
    d.J0-=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j0_min=1;
}

void MainWindow::on_pushButton_j0_plus_pressed()
{
    d.J0+=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j0_plus=1;
}

void MainWindow::on_pushButton_j1_min_pressed()
{
    d.J1-=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j1_min=1;
}

void MainWindow::on_pushButton_j1_plus_pressed()
{
    d.J1+=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j1_plus=1;
}

void MainWindow::on_pushButton_j2_min_pressed()
{
    d.J2-=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j2_min=1;
}

void MainWindow::on_pushButton_j2_plus_pressed()
{
    d.J2+=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j2_plus=1;
}

void MainWindow::on_pushButton_j3_min_pressed()
{
    d.J3-=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j3_min=1;
}

void MainWindow::on_pushButton_j3_plus_pressed()
{
    d.J3+=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j3_plus=1;
}

void MainWindow::on_pushButton_j4_min_pressed()
{
    d.J4-=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j4_min=1;
}

void MainWindow::on_pushButton_j4_plus_pressed()
{
    d.J4+=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j4_plus=1;
}

void MainWindow::on_pushButton_j5_min_pressed()
{
    d.J5-=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j5_min=1;
}

void MainWindow::on_pushButton_j5_plus_pressed()
{
    d.J5+=d.stepsize_joint*toRadians;
    d=myKin->fk(d);
    d.motion_j5_plus=1;
}


void MainWindow::on_pushButton_euler_x_min_pressed()
{
    d.Eulerx-=d.stepsize_euler*toRadians;
    d=myKin->ik(d);
    d.motion_euler_x_min=1;
}

void MainWindow::on_pushButton_euler_x_plus_pressed()
{
    d.Eulerx+=d.stepsize_euler*toRadians;
    d=myKin->ik(d);
    d.motion_euler_x_plus=1;
}

void MainWindow::on_pushButton_euler_y_min_pressed()
{
    d.Eulery-=d.stepsize_euler*toRadians;
    d=myKin->ik(d);
    d.motion_euler_y_min=1;
}

void MainWindow::on_pushButton_euler_y_plus_pressed()
{
    d.Eulery+=d.stepsize_euler*toRadians;
    d=myKin->ik(d);
    d.motion_euler_y_plus=1;
}

void MainWindow::on_pushButton_euler_z_min_pressed()
{
    d.Eulerz-=d.stepsize_euler*toRadians;
    d=myKin->ik(d);
    d.motion_euler_z_min=1;
}

void MainWindow::on_pushButton_euler_z_plus_pressed()
{
    d.Eulerz+=d.stepsize_euler*toRadians;
    d=myKin->ik(d);
    d.motion_euler_z_plus=1;
}

void MainWindow::on_pushButton_tool_x_min_pressed()
{
    d.Toolx=d.stepsize_tooldir*-1;
    d.Tooly=0;
    d.Toolz=0;
    //! Save euler angles.
    double eulx=d.Eulerx;
    double euly=d.Eulery;
    double eulz=d.Eulerz;
    //! New target xyz coordinates.
    d=myKin->ik(d);
    d.Cartx=d.Toolx;
    d.Carty=d.Tooly;
    d.Cartz=d.Toolz;

    d.Toolx=0;
    d.Tooly=0;
    d.Toolz=0;

    //! Avoid different euler angles.
    d.Eulerx=eulx;
    d.Eulery=euly;
    d.Eulerz=eulz;
    //! Set target position.
    d=myKin->ik(d);
    d.motion_tool_x_min=1;
}

void MainWindow::on_pushButton_tool_x_plus_pressed()
{
    d.Toolx=d.stepsize_tooldir;
    d.Tooly=0;
    d.Toolz=0;
    //! Save euler angles.
    double eulx=d.Eulerx;
    double euly=d.Eulery;
    double eulz=d.Eulerz;
    //! New target xyz coordinates.
    d=myKin->ik(d);
    d.Cartx=d.Toolx;
    d.Carty=d.Tooly;
    d.Cartz=d.Toolz;

    d.Toolx=0;
    d.Tooly=0;
    d.Toolz=0;

    //! Avoid different euler angles.
    d.Eulerx=eulx;
    d.Eulery=euly;
    d.Eulerz=eulz;
    //! Set target position.
    d=myKin->ik(d);
    d.motion_tool_x_plus=1;
}

void MainWindow::on_pushButton_tool_y_min_pressed()
{
    d.Toolx=0;
    d.Tooly=d.stepsize_tooldir*-1;
    d.Toolz=0;
    //! Save euler angles.
    double eulx=d.Eulerx;
    double euly=d.Eulery;
    double eulz=d.Eulerz;
    //! New target xyz coordinates.
    d=myKin->ik(d);
    d.Cartx=d.Toolx;
    d.Carty=d.Tooly;
    d.Cartz=d.Toolz;

    d.Toolx=0;
    d.Tooly=0;
    d.Toolz=0;
    //! Avoid different euler angles.
    d.Eulerx=eulx;
    d.Eulery=euly;
    d.Eulerz=eulz;
    //! Set target position.
    d=myKin->ik(d);
    d.motion_tool_y_min=1;
}

void MainWindow::on_pushButton_tool_y_plus_pressed()
{
    d.Toolx=0;
    d.Tooly=d.stepsize_tooldir;
    d.Toolz=0;
    //! Save euler angles.
    double eulx=d.Eulerx;
    double euly=d.Eulery;
    double eulz=d.Eulerz;
    //! New target xyz coordinates.
    d=myKin->ik(d);
    d.Cartx=d.Toolx;
    d.Carty=d.Tooly;
    d.Cartz=d.Toolz;

    d.Toolx=0;
    d.Tooly=0;
    d.Toolz=0;
    //! Avoid different euler angles.
    d.Eulerx=eulx;
    d.Eulery=euly;
    d.Eulerz=eulz;
    //! Set target position.
    d=myKin->ik(d);
    d.motion_tool_y_plus=1;
}

void MainWindow::on_pushButton_tool_z_min_pressed()
{
    d.Toolx=0;
    d.Tooly=0;
    d.Toolz=d.stepsize_tooldir*-1;
    //! Save euler angles.
    double eulx=d.Eulerx;
    double euly=d.Eulery;
    double eulz=d.Eulerz;
    //! New target xyz coordinates.
    d=myKin->ik(d);
    d.Cartx=d.Toolx;
    d.Carty=d.Tooly;
    d.Cartz=d.Toolz;

    d.Toolx=0;
    d.Tooly=0;
    d.Toolz=0;
    //! Avoid different euler angles.
    d.Eulerx=eulx;
    d.Eulery=euly;
    d.Eulerz=eulz;
    //! Set target position.
    d=myKin->ik(d);
    d.motion_tool_z_min=1;
}

void MainWindow::on_pushButton_tool_z_plus_pressed()
{
    d.Toolx=0;
    d.Tooly=0;
    d.Toolz=d.stepsize_tooldir;
    //! Save euler angles.
    double eulx=d.Eulerx;
    double euly=d.Eulery;
    double eulz=d.Eulerz;
    //! New target xyz coordinates.
    d=myKin->ik(d);
    d.Cartx=d.Toolx;
    d.Carty=d.Tooly;
    d.Cartz=d.Toolz;

    d.Toolx=0;
    d.Tooly=0;
    d.Toolz=0;
    //! Avoid different euler angles.
    d.Eulerx=eulx;
    d.Eulery=euly;
    d.Eulerz=eulz;
    //! Set target position.
    d=myKin->ik(d);
    d.motion_tool_z_plus=1;
}

void MainWindow::on_spinBox_stepsize_cart_valueChanged(int arg1)
{
    d.stepsize_cart=arg1;
}

void MainWindow::on_spinBox_stepsize_euler_valueChanged(int arg1)
{
    d.stepsize_euler=arg1;
}

void MainWindow::on_spinBox_stepsize_tooldir_valueChanged(int arg1)
{
    d.stepsize_tooldir=arg1;
}

void MainWindow::on_spinBox_stepsize_joint_valueChanged(int arg1)
{
    d.stepsize_joint=arg1;
}

void MainWindow::on_checkBox_ik_from_init_toggled(bool checked)
{
    d.Ik_from_init=checked;
}
//! Inverse kinematics iterations to get solution.
void MainWindow::on_spinBox_ik_iterations_valueChanged(int arg1)
{
    d.iterations=arg1;
}
//! Reset machine position.
void MainWindow::on_pushButton_reset_pressed()
{
    d.J0=0;
    d.J1=0;
    d.J2=0;
    d.J3=0;
    d.J4=0;
    d.J5=0;
    d=myKin->fk(d);
}
//! Stop continious motion callbacks.
void MainWindow::on_checkBox_motion_continious_toggled(bool checked)
{
    d.motion_continious=checked;
}

void MainWindow::on_pushButton_x_min_released()
{
    d.motion_x_min=0;
}

void MainWindow::on_pushButton_x_plus_released()
{
    d.motion_x_plus=0;
}

void MainWindow::on_pushButton_y_min_released()
{
    d.motion_y_min=0;
}

void MainWindow::on_pushButton_y_plus_released()
{
    d.motion_y_plus=0;
}

void MainWindow::on_pushButton_z_min_released()
{
    d.motion_z_min=0;
}

void MainWindow::on_pushButton_z_plus_released()
{
    d.motion_z_plus=0;
}

void MainWindow::on_pushButton_j0_min_released()
{
    d.motion_j0_min=0;
}

void MainWindow::on_pushButton_j0_plus_released()
{
    d.motion_j0_plus=0;
}

void MainWindow::on_pushButton_j1_min_released()
{
    d.motion_j1_min=0;
}

void MainWindow::on_pushButton_j1_plus_released()
{
    d.motion_j1_plus=0;
}

void MainWindow::on_pushButton_j2_min_released()
{
    d.motion_j2_min=0;
}

void MainWindow::on_pushButton_j2_plus_released()
{
    d.motion_j2_plus=0;
}

void MainWindow::on_pushButton_j3_min_released()
{
    d.motion_j3_min=0;
}

void MainWindow::on_pushButton_j3_plus_released()
{
    d.motion_j3_plus=0;
}

void MainWindow::on_pushButton_j4_min_released()
{
    d.motion_j4_min=0;
}

void MainWindow::on_pushButton_j4_plus_released()
{
    d.motion_j4_plus=0;
}

void MainWindow::on_pushButton_j5_min_released()
{
    d.motion_j5_min=0;
}

void MainWindow::on_pushButton_j5_plus_released()
{
    d.motion_j5_plus=0;
}

void MainWindow::on_pushButton_euler_x_min_released()
{
    d.motion_euler_x_min=0;
}

void MainWindow::on_pushButton_euler_x_plus_released()
{
    d.motion_euler_x_plus=0;
}

void MainWindow::on_pushButton_euler_y_min_released()
{
    d.motion_euler_y_min=0;
}

void MainWindow::on_pushButton_euler_y_plus_released()
{
    d.motion_euler_y_plus=0;
}

void MainWindow::on_pushButton_euler_z_min_released()
{
    d.motion_euler_z_min=0;
}

void MainWindow::on_pushButton_euler_z_plus_released()
{
    d.motion_euler_z_plus=0;
}

void MainWindow::on_pushButton_tool_x_min_released()
{
    d.motion_tool_x_min=0;
}

void MainWindow::on_pushButton_tool_x_plus_released()
{
    d.motion_tool_x_plus=0;
}

void MainWindow::on_pushButton_tool_y_min_released()
{
    d.motion_tool_y_min=0;
}

void MainWindow::on_pushButton_tool_y_plus_released()
{
    d.motion_tool_y_plus=0;
}

void MainWindow::on_pushButton_tool_z_min_released()
{
    d.motion_tool_z_min=0;
}

void MainWindow::on_pushButton_tool_z_plus_released()
{
    d.motion_tool_z_plus=0;
}
