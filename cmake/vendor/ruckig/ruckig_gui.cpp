#include "ruckig_gui.h"
#include "ui_ruckig_gui.h"

ruckig_traject::RUCKIG r;

ruckig_gui::ruckig_gui(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ruckig_gui)
{
    ui->setupUi(this);
    t = new std::thread(&ruckig_gui::thread,this);
    //t->detach();

    r.demo=1;

    ui->doubleSpinBox_machine_maxvel->setValue(250);
    ui->doubleSpinBox_machine_maxacc->setValue(200);
    ui->doubleSpinBox_machine_maxjerk->setValue(150);

    r.maxvel_machine=ui->doubleSpinBox_machine_maxvel->value();
    r.maxacc_machine=ui->doubleSpinBox_machine_maxacc->value();
    r.maxjerk_machine=ui->doubleSpinBox_machine_maxjerk->value();
    r.feed_overide=ui->doubleSpinBox_feed_override->value();

    ui->pushButton_motion_stop_resume->setStyleSheet("background-color: rgb(0, 255, 127);");
}

ruckig_gui::~ruckig_gui()
{
    delete ui;
}

void ruckig_gui::thread(){

    using namespace std::chrono;
    using clock = steady_clock;

    auto time_up = clock::now() + milliseconds(1);

    while(true)
    {
        std::this_thread::sleep_until(time_up);

        //! Traject runner.
        int ok=ruckig_traject().traject(r);
        if(!ok){std::cout<<"ruckig error"<<std::endl;}

        //! Print results in gui.
        ui->lineEdit_current_line->setText(QString::number(r.i));
        ui->lineEdit_pahtlenght->setText(QString::number(r.waypointvec.at(r.i).pathlenght,'f',3));
        ui->lineEdit_distance_to_go->setText(QString::number(r.dtg,'f',3));

        ui->lineEdit_current_position->setText(QString::number(r.pos[0],'f',3));
        ui->lineEdit_current_velocity->setText(QString::number(r.vel[0],'f',3));
        ui->lineEdit_current_acceleration->setText(QString::number(r.acc[0],'f',3));

        ui->lineEdit_current_traject_duration->setText(QString::number(r.tr_duration,'f',3));
        ui->lineEdit_current_path_duration->setText(QString::number(r.waypointvec.at(r.i).wp_duration,'f',3));

        ui->lineEdit_at_traject_start->setText(QString::number(r.at_start));
        ui->lineEdit_at_traject_finish->setText(QString::number(r.at_finish));

        ui->lineEdit_traject_vector_size->setText(QString::number(r.waypointvec.size()));

        ui->lineEdit_path_start_position->setText(QString::number(r.waypointvec.at(r.i).startpos,'f',3));
        ui->lineEdit_path_end_position->setText(QString::number(r.waypointvec.at(r.i).endpos,'f',3));
        ui->lineEdit_path_start_velocity->setText(QString::number(r.waypointvec.at(r.i).startvel,'f',3));
        ui->lineEdit_path_end_velocity->setText(QString::number(r.waypointvec.at(r.i).endvel,'f',3));

        ui->lineEdit_path_max_velocity->setText(QString::number(r.waypointvec.at(r.i).maxvel,'f',3));
        ui->lineEdit_path_max_acceleration->setText(QString::number(r.waypointvec.at(r.i).maxacc,'f',3));
        ui->lineEdit_path_max_jerk->setText(QString::number(r.waypointvec.at(r.i).maxjerk,'f',3));

        //! set time_point for next millisecond
        time_up += milliseconds(10);
    }

}

void ruckig_gui::on_pushButton_motion_stop_resume_pressed()
{
    r.motion_stop=!r.motion_stop;
    if(r.motion_stop){
        ui->pushButton_motion_stop_resume->setStyleSheet("background-color: rgb(255, 170, 0);");
    } else {
        ui->pushButton_motion_stop_resume->setStyleSheet("background-color: rgb(0, 255, 127);");
    }
}

void ruckig_gui::on_doubleSpinBox_machine_maxvel_valueChanged(double arg1)
{
    r.maxvel_machine=arg1;
}

void ruckig_gui::on_doubleSpinBox_machine_maxacc_valueChanged(double arg1)
{
    r.maxacc_machine=arg1;
}

void ruckig_gui::on_doubleSpinBox_machine_maxjerk_valueChanged(double arg1)
{
    r.maxjerk_machine=arg1;
}

void ruckig_gui::on_doubleSpinBox_feed_override_valueChanged(double arg1)
{
     r.feed_overide=arg1;
}
