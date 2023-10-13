void on_doubleSpinBox_tarpos_valueChanged(double arg1);
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <iostream>
#include <opengl.h>

#include "sc_ruckig.h"

typedef double T;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void opengl_clear();

    void opengl_add(T pos, T vel,T acc);

private slots:

    void thread();

    void on_pushButton_run_pressed();

    void on_pushButton_pause_pressed();

    void on_doubleSpinBox_tarpos_valueChanged(double arg1);

    void on_doubleSpinBox_vm_valueChanged(double arg1);

    void on_doubleSpinBox_a_valueChanged(double arg1);

    void on_doubleSpinBox_ve_valueChanged(double arg1);

    void on_doubleSpinBox_jm_valueChanged(double arg1);

    void on_doubleSpinBox_vo_valueChanged(double arg1);

    void on_doubleSpinBox_s_valueChanged(double arg1);

    void on_doubleSpinBox_acs_valueChanged(double arg1);

    void on_doubleSpinBox_ace_valueChanged(double arg1);

    void on_pushButton_set_mempos_zero_pressed();

    void on_doubleSpinBox_af_valueChanged(double arg1);

    void on_doubleSpinBox_fo_valueChanged(double arg1);

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    opengl *myOpenGl;

    sc_ruckig *ruck=new sc_ruckig();
};
#endif
