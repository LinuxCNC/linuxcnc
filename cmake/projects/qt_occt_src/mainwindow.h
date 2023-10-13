#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <iostream>

//! Make conversion's easy:
#define toRadians M_PI/180.0
#define toDegrees (180.0/M_PI)

#ifdef Success
#undef Success
#endif

//! libocc
#include <libocct/opencascade.h>
using namespace occ;

//! libspline
#include <libspline/cubic_spline.h>

//! libdxfrw
#include <libdxfrw/libdxfrw_functions.h>

//! libkinematic
#include <libkinematic/kinematic.h>

//! hal
//#include <hal/halio.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    Opencascade* OpencascadeWidget;

    void update();
    void update_opencascade();

    void checkStatus();
    void checkContiniousMotion();

private slots:

    void on_pushButton_x_min_pressed();

    void on_pushButton_x_plus_pressed();

    void on_pushButton_y_min_pressed();

    void on_pushButton_y_plus_pressed();

    void on_pushButton_z_min_pressed();

    void on_pushButton_z_plus_pressed();

    void on_pushButton_j0_min_pressed();

    void on_pushButton_j0_plus_pressed();

    void on_pushButton_j1_min_pressed();

    void on_pushButton_j1_plus_pressed();

    void on_pushButton_j2_min_released();

    void on_pushButton_j2_plus_pressed();

    void on_pushButton_j3_min_released();

    void on_pushButton_j3_plus_pressed();

    void on_pushButton_j4_min_pressed();

    void on_pushButton_j4_plus_pressed();

    void on_pushButton_j5_min_pressed();

    void on_pushButton_j5_plus_pressed();

    void on_pushButton_euler_x_min_pressed();

    void on_pushButton_euler_x_plus_pressed();

    void on_pushButton_euler_y_min_pressed();

    void on_pushButton_euler_y_plus_pressed();

    void on_pushButton_euler_z_min_pressed();

    void on_pushButton_euler_z_plus_pressed();

    void on_pushButton_tool_x_min_pressed();

    void on_pushButton_tool_x_plus_pressed();

    void on_pushButton_tool_y_min_pressed();

    void on_pushButton_tool_y_plus_pressed();

    void on_pushButton_tool_z_min_pressed();

    void on_pushButton_tool_z_plus_pressed();

    void on_spinBox_stepsize_cart_valueChanged(int arg1);

    void on_spinBox_stepsize_euler_valueChanged(int arg1);

    void on_spinBox_stepsize_tooldir_valueChanged(int arg1);

    void on_spinBox_stepsize_joint_valueChanged(int arg1);

    void on_checkBox_ik_from_init_toggled(bool checked);

    void on_spinBox_ik_iterations_valueChanged(int arg1);

    void on_pushButton_reset_pressed();

    void on_checkBox_motion_continious_toggled(bool checked);

    void on_pushButton_x_min_released();

    void on_pushButton_x_plus_released();

    void on_pushButton_y_min_released();

    void on_pushButton_y_plus_released();

    void on_pushButton_z_min_released();

    void on_pushButton_z_plus_released();

    void on_pushButton_j0_min_released();

    void on_pushButton_j0_plus_released();

    void on_pushButton_j1_min_released();

    void on_pushButton_j1_plus_released();

    void on_pushButton_j2_plus_released();

    void on_pushButton_j3_plus_released();

    void on_pushButton_j4_min_released();

    void on_pushButton_j4_plus_released();

    void on_pushButton_j5_min_released();

    void on_pushButton_j5_plus_released();

    void on_pushButton_j2_min_pressed();

    void on_pushButton_j3_min_pressed();

    void on_pushButton_euler_x_min_released();

    void on_pushButton_euler_x_plus_released();

    void on_pushButton_euler_y_min_released();

    void on_pushButton_euler_y_plus_released();

    void on_pushButton_euler_z_min_released();

    void on_pushButton_euler_z_plus_released();

    void on_pushButton_tool_x_min_released();

    void on_pushButton_tool_x_plus_released();

    void on_pushButton_tool_y_min_released();

    void on_pushButton_tool_y_plus_released();

    void on_pushButton_tool_z_min_released();

    void on_pushButton_tool_z_plus_released();

private:
    Ui::MainWindow *ui;
    //halio *myHal=new halio();
    Kinematic *myKin=new Kinematic();
    struct data d;
};
#endif
