#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //! This activates a screen update when robot is moving and screen needs to be updated automaticly.
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::update);
    timer->start(100);

    ui->label_jog_speed->setText(QString::number(ui->horizontalScrollBar_jog_speed->value(),'f',3));

    ui->label_rapid_override->setText(QString::number(ui->horizontalScrollBar_rapid_override->value(),'f',3));
    ui->label_spindle_override->setText(QString::number(ui->horizontalScrollBar_spindle_override->value(),'f',3));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::update(){
    myNml->update();

    if(myNml->theStatus.estop){
        ui->pushButton_estop->setChecked(true);
    } else {
        ui->pushButton_estop->setChecked(false);
    }


    if(myNml->theStatus.machine_on){
        ui->pushButton_machine_on->setChecked(true);
    } else {
        ui->pushButton_machine_on->setChecked(false);
    }

    ui->label_active_gcodes->setText(myNml->theStatus.task_active_gcodes_string);
    ui->label_active_mcodes->setText(myNml->theStatus.task_active_mcodes_string);
    ui->label_active_fcodes->setText(myNml->theStatus.task_active_fcodes_string);
    ui->label_active_scodes->setText(myNml->theStatus.task_active_scodes_string);

    QString mode;
    if(myNml->theStatus.mode==0 && myNml->theStatus.machine_on){
        mode="manual";
        ui->pushButton_manual->setStyleSheet(green);
        ui->pushButton_mdi->setStyleSheet(grey);
        ui->pushButton_auto->setStyleSheet(grey);
    } else
    if(myNml->theStatus.mode==1 && myNml->theStatus.machine_on){
        mode="mdi";
        ui->pushButton_manual->setStyleSheet(grey);
        ui->pushButton_mdi->setStyleSheet(green);
        ui->pushButton_auto->setStyleSheet(grey);
    } else
    if(myNml->theStatus.mode==2 && myNml->theStatus.machine_on){
        mode="auto";
        ui->pushButton_manual->setStyleSheet(grey);
        ui->pushButton_mdi->setStyleSheet(grey);
        ui->pushButton_auto->setStyleSheet(green);
    } else {
        ui->pushButton_manual->setStyleSheet(grey);
        ui->pushButton_mdi->setStyleSheet(grey);
        ui->pushButton_auto->setStyleSheet(grey);
    }
    ui->label_mode->setText(mode);

    ui->label_x_coordinate->setText(QString::number(myNml->theStatus.x,'f',3));
    ui->label_y_coordinate->setText(QString::number(myNml->theStatus.y,'f',3));
    ui->label_z_coordinate->setText(QString::number(myNml->theStatus.z,'f',3));

    ui->label_feed_override_status->setText(QString::number(myNml->theStatus.feed_override*100,'f',3));
    if((myNml->theStatus.feed_override*100)!=ui->horizontalScrollBar_feed_override->value()){
        ui->horizontalScrollBar_feed_override->setValue(myNml->theStatus.feed_override*100);
    }

    ui->label_rapid_override->setText(QString::number(myNml->theStatus.rapid_override*100,'f',3));
    if((myNml->theStatus.rapid_override*100)!=ui->horizontalScrollBar_rapid_override->value()){
        ui->horizontalScrollBar_rapid_override->setValue(myNml->theStatus.rapid_override*100);
    }

    ui->label_max_velocity->setText(QString::number(myNml->theStatus.max_velocity,'f',3));
    if((myNml->theStatus.max_velocity)!=ui->horizontalScrollBar_max_velocity->value()){
        ui->horizontalScrollBar_max_velocity->setValue(myNml->theStatus.max_velocity);
    }

    ui->label_current_velocity->setText(QString::number(myNml->theStatus.current_velocity,'f',3));
    ui->label_rpm_status->setText(QString::number(myNml->theStatus.current_rpm,'f',3));
    ui->label_spindle_override_status->setText(QString::number(myNml->theStatus.spindle_override*100,'f',3));
    ui->label_current_line->setText(QString::number(myNml->theStatus.current_line,'f',0));
    ui->label_motion_line->setText(QString::number(myNml->theStatus.motion_line,'f',0));

    ui->label_command->setText(myNml->theStatus.command);
    ui->label_inifile->setText(myNml->theStatus.inifile);
    ui->label_command->setText(myNml->theStatus.command);

    if(myNml->theStatus.homed_x){
        ui->label_homed_x->setText("H");
    } else {
        ui->label_homed_x->setText("-");
    }

    if(myNml->theStatus.homed_y){
        ui->label_homed_y->setText("H");
    } else {
        ui->label_homed_y->setText("-");
    }

    if(myNml->theStatus.homed_z){
        ui->label_homed_z->setText("H");
    } else {
        ui->label_homed_z->setText("-");
    }

    if(myNml->theStatus.homed_all){
        ui->label_homed_all->setText("H");
    } else {
        ui->label_homed_all->setText("-");
    }

    if(myNml->theStatus.estop){
        ui->label_estop_status->setText("true");
        ui->pushButton_estop->setStyleSheet(red);
    } else {
        ui->label_estop_status->setText("false");
        ui->pushButton_estop->setStyleSheet(green);
    }

    if(myNml->theStatus.machine_on){
        ui->label_machine_on_off_status->setText("on");
        ui->pushButton_machine_on->setStyleSheet(green);
    } else {
        ui->label_machine_on_off_status->setText("off");
        ui->pushButton_machine_on->setStyleSheet(orange);
    }

    if(myNml->theStatus.mode==2){
        ui->lineEdit_run_from_line->setText(QString::number(myNml->theStatus.motion_line,'f',0));
    }

    if(myNml->theStatus.run){
        ui->pushButton_run->setStyleSheet(green);
         ui->pushButton_resume->setStyleSheet(grey);
             ui->pushButton_pause->setStyleSheet(grey);
                 ui->pushButton_stop->setStyleSheet(grey);
    } else {
        ui->pushButton_run->setStyleSheet(grey);
    }
    if(myNml->theStatus.pause){
        ui->pushButton_pause->setStyleSheet(orange);
    } else {
        ui->pushButton_pause->setStyleSheet(grey);
    }
    if(myNml->theStatus.idle){
        ui->pushButton_stop->setStyleSheet(red);
    } else {
        ui->pushButton_stop->setStyleSheet(grey);
    }

}

void MainWindow::on_pushButton_estop_pressed()
{
    //! Toggle.
    if(myNml->theStatus.estop){
        myNml->estop_reset();
    } else {
        myNml->estop();
    }
}
void MainWindow::on_pushButton_machine_on_pressed()
{
    //! Toggle.
    if(myNml->theStatus.machine_on){
        myNml->machine_off();
    } else {
        myNml->machine_on();
    }
}
void MainWindow::on_pushButton_load_pressed()
{
    myNml->load("");
}

void MainWindow::on_pushButton_auto_pressed()
{
    myNml->mode_auto();
}

void MainWindow::on_pushButton_mdi_pressed()
{
    myNml->mode_mdi();
}

void MainWindow::on_pushButton_manual_pressed()
{
    myNml->mode_manual();
}

void MainWindow::on_pushButton_run_pressed()
{
    myNml->mode_auto();
    myNml->run(ui->lineEdit_run_from_line->text().toUInt());
}
void MainWindow::on_pushButton_resume_pressed()
{
    myNml->resume();
}

void MainWindow::on_pushButton_reverse_pressed()
{
    myNml->reverse();
}

void MainWindow::on_pushButton_pause_pressed()
{
    myNml->pause();
}

void MainWindow::on_pushButton_teleop_pressed()
{
    myNml->teleop();
}

void MainWindow::on_pushButton_coord_pressed()
{
    myNml->coord();
}

void MainWindow::on_pushButton_free_pressed()
{
    myNml->free();
}

void MainWindow::on_pushButton_home_x_pressed()
{
    myNml->home_x();
}

void MainWindow::on_pushButton_home_y_pressed()
{
    myNml->home_y();
}

void MainWindow::on_pushButton_home_z_pressed()
{
    myNml->home_z();
}

void MainWindow::on_pushButton_stop_pressed()
{
    myNml->stop();
    myNml->mode_manual();
}

void MainWindow::on_pushButton_spindle_off_pressed()
{
    myNml->spindle_off(0); //! Spindle nr.
}

void MainWindow::on_pushButton_spindle_on_cw_pressed()
{
    myNml->spindle_on(0,ui->horizontalScrollBar_spindle_rpm->value());
}

void MainWindow::on_pushButton_spindle_on_ccw_pressed()
{
    myNml->spindle_on(0,-abs(ui->horizontalScrollBar_spindle_rpm->value()));
}

void MainWindow::on_pushButton_jog_x_min_pressed()
{
    myNml->jog(0,ui->horizontalScrollBar_jog_speed->value()*-1);
}

void MainWindow::on_pushButton_jog_x_min_released()
{
    //myNml->jog(0,0);
    myNml->jog_stop(0);
}

void MainWindow::on_pushButton_jog_x_plus_pressed()
{
    myNml->jog(0,ui->horizontalScrollBar_jog_speed->value());
}

void MainWindow::on_pushButton_jog_x_plus_released()
{
    //myNml->jog(0,0);
    myNml->jog_stop(0);
}

void MainWindow::on_pushButton_jog_y_min_pressed()
{
    myNml->jog(1,ui->horizontalScrollBar_jog_speed->value()*-1);
}

void MainWindow::on_pushButton_jog_y_min_released()
{
    //myNml->jog(1,0);
    myNml->jog_stop(1);
}

void MainWindow::on_pushButton_jog_y_plus_pressed()
{
    myNml->jog(1,ui->horizontalScrollBar_jog_speed->value());
}

void MainWindow::on_pushButton_jog_y_plus_released()
{
    //myNml->jog(1,0);
    myNml->jog_stop(1);
}

void MainWindow::on_pushButton_jog_z_min_pressed()
{
    myNml->jog(2,ui->horizontalScrollBar_jog_speed->value()*-1);
}

void MainWindow::on_pushButton_jog_z_min_released()
{
    //myNml->jog(2,0);
    myNml->jog_stop(2);
}

void MainWindow::on_pushButton_jog_z_plus_pressed()
{
    myNml->jog(2,ui->horizontalScrollBar_jog_speed->value());
}

void MainWindow::on_pushButton_jog_z_plus_released()
{
    //myNml->jog(2,0);
    myNml->jog_stop(2);
}

void MainWindow::on_horizontalScrollBar_jog_speed_valueChanged(int value)
{
    ui->label_jog_speed->setText(QString::number(value,'f',3));
}

void MainWindow::on_pushButton_home_all_pressed()
{
    myNml->home_all();
}

void MainWindow::on_pushButton_unhome_all_pressed()
{
    myNml->unhome_all();
}

void MainWindow::on_horizontalScrollBar_feed_override_valueChanged(int value)
{
    myNml->setFeedOveride(value*0.01);
    ui->label_feed_overide->setText(QString::number(value,'f',3));
}

void MainWindow::on_horizontalScrollBar_max_velocity_valueChanged(int value)
{
    myNml->setMaxVelocity(value);
    ui->label_max_velocity->setText(QString::number(value,'f',3));
}

void MainWindow::on_horizontalScrollBar_rapid_override_valueChanged(int value)
{
    myNml->setRapidOverride(value*0.01);
    ui->label_rapid_override->setText(QString::number(value,'f',3));
}

void MainWindow::on_horizontalScrollBar_spindle_rpm_valueChanged(int value)
{
    ui->label_spindle_rpm->setText(QString::number(value,'f',3));
}

void MainWindow::on_horizontalScrollBar_spindle_override_valueChanged(int value)
{
    myNml->setSpindleOverride(1,value*0.01);
    ui->label_spindle_override->setText(QString::number(value,'f',3));
}

void MainWindow::on_lineEdit_mdi_input_returnPressed()
{
    myNml->mode_mdi();
    myNml->mdi(ui->lineEdit_mdi_input->text().toStdString());
}

void MainWindow::on_pushButton_goto_zero_pressed()
{
    myNml->stop();
    myNml->mode_mdi();
    myNml->mdi("G0 X0 Y0 Z0");
}

void MainWindow::on_pushButton_unhome_x_pressed()
{
    myNml->unhome_x();
}

void MainWindow::on_pushButton_unhome_y_pressed()
{
    myNml->unhome_y();
}

void MainWindow::on_pushButton_unhome_z_pressed()
{
    myNml->unhome_z();
}

void MainWindow::on_pushButton_mdi_exec_pressed()
{
    on_lineEdit_mdi_input_returnPressed();
}

void MainWindow::on_pushButton_run_step_pressed()
{
   myNml->run_step();
}

void MainWindow::on_pushButton_forward_pressed()
{
    myNml->forward();
}
