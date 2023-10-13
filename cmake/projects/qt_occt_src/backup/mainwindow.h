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

//! hal
#include <hal/halio.h>

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

    void Update_Opencascade();
    void gcode();


private slots:
    void on_pushButton_pressed();

    void on_pushButton_2_pressed();

private:
    Ui::MainWindow *ui;
    halio *myHal=new halio();
};
#endif
