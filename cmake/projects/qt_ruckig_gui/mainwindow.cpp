#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "ruckig.hpp"

typedef double T;
typedef bool B;
std::vector<T> vvec,svec,avec;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //! OpenGl output to verify.
    myOpenGl = new opengl();
    //! Graph scale.
    myOpenGl->setScale(25,25);
    myOpenGl->setInterval(0.001);
    myOpenGl->set2VecShift(100);
    myOpenGl->set1VecScale(0.1);

    //! Timer to simulate servo cycle.
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(thread()));
    timer->start(1);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_run_pressed()
{
    /*
    ruck->set_a_jm(ui->doubleSpinBox_a->value(),
                   ui->doubleSpinBox_jm->value());
    ruck->set_target(ui->doubleSpinBox_s->value(),0,0);
    ruck->set_vm(ui->doubleSpinBox_vm->value());
    ruck->set_fo(ui->doubleSpinBox_fo->value());
    ruck->set_af(ui->doubleSpinBox_af->value());
    ruck->run();*/

    ruck->set_all_and_run(ui->doubleSpinBox_a->value(),
                          ui->doubleSpinBox_jm->value(),
                          0.001,
                          ui->doubleSpinBox_vm->value(),
                          ui->doubleSpinBox_fo->value(),
                          ui->doubleSpinBox_af->value(),
                          0, ui->doubleSpinBox_vo->value(), 0,
                          ui->doubleSpinBox_s->value(), ui->doubleSpinBox_ve->value(), 0);
}

void MainWindow::on_pushButton_pause_pressed()
{
    ruck->stop();
}

void MainWindow::on_pushButton_set_mempos_zero_pressed()
{
    ruck->set_mem_pos(0);
}

//! This function simulates the servo cycle. And is called every 1 millisecond.
void MainWindow::thread(){

    T p,v,a;
    T pos_increment=0;
    T pos_memory=0;

    ruck->update(pos_increment,pos_memory,p,v,a);

    switch (ruck->get_state()) {
    case sc_ruckig_run:
        opengl_add(pos_memory,v,a);
        break;
    case sc_ruckig_stop:
        opengl_add(pos_memory,v,a);
        break;
    case sc_ruckig_wait:

        break;
    case sc_ruckig_finished:
        //! Increment position.
        // ui->doubleSpinBox_s->setValue(pos_memory+10);
        // opengl_clear();
        break;
    default:
        break;
    }

    std::cout<<"pos:"<<pos_memory<<std::endl;
    std::cout<<"vel:"<<v<<std::endl;
    std::cout<<"acc:"<<a<<std::endl;
}

void MainWindow::opengl_clear(){
    vvec.clear();
    svec.clear();
    avec.clear();
    myOpenGl->setj0vec(vvec);
    myOpenGl->setj1vec(svec);
    myOpenGl->setj2vec(avec);
}

void MainWindow::opengl_add(T pos, T vel,T acc){
    svec.push_back(pos);
    vvec.push_back(vel);
    avec.push_back(acc);
    myOpenGl->setj0vec(vvec);
    myOpenGl->setj1vec(svec);
    myOpenGl->setj2vec(avec);
}

void MainWindow::on_doubleSpinBox_tarpos_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_vm_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_a_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_jm_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_vo_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_ve_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_s_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_acs_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_ace_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_af_valueChanged(double arg1)
{
    // on_pushButton_run_pressed();
}

void MainWindow::on_doubleSpinBox_fo_valueChanged(double arg1)
{
    //on_pushButton_run_pressed();
}
