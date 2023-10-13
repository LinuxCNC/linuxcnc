#ifndef RUCKIG_GUI_H
#define RUCKIG_GUI_H

#include <QMainWindow>
#include "ruckig_traject.h"
#include "iostream"
#include "thread"
#include "chrono"

QT_BEGIN_NAMESPACE
namespace Ui { class ruckig_gui; }
QT_END_NAMESPACE

class ruckig_gui : public QMainWindow
{
    Q_OBJECT

public:
    ruckig_gui(QWidget *parent = nullptr);
    ~ruckig_gui();
    void thread();

private slots:
    void on_pushButton_motion_stop_resume_pressed();

    void on_doubleSpinBox_machine_maxvel_valueChanged(double arg1);

    void on_doubleSpinBox_machine_maxacc_valueChanged(double arg1);

    void on_doubleSpinBox_machine_maxjerk_valueChanged(double arg1);

    void on_doubleSpinBox_feed_override_valueChanged(double arg1);

private:
    Ui::ruckig_gui *ui;
    std::thread *t;
};
#endif // RUCKIG_GUI_H
