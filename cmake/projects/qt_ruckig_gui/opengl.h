#ifndef OPENGL_H
#define OPENGL_H

#include <QMainWindow>
#include <mainwindow.h>
#include <QWidget>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QToolTip>
#include <GL/gl.h>
#include <GL/glu.h>
#include <QtWidgets>

class opengl : public QOpenGLWidget{

public:
    opengl(QWidget *parent = nullptr);
    ~opengl();

    void clearvec();

    void setInterval(double theInterval);
    void setScale(double thexScale, double theyScale);
    void setj0vec(std::vector<double> theyVec);
    void setj1vec(std::vector<double> theyVec);
    void setj2vec(std::vector<double> theyVec);
    void setj3Vec(std::vector<double> theyVec);

    void set2VecShift(double theyShift);

    void set1VecScale(double theScale);
    void initializeGL();
    void resizeGL(int w, int h);
    void paintGL();

private:
    opengl *myOpengl;
};

#endif
