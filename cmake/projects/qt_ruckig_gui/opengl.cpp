#include <math.h>
#include <mainwindow.h>
#include <iostream>
#include <opengl.h>
#include <vector>

double xScale=0;
double yScale=0;
double t=0;
double interval=0.01;
double yShift=0;
double scale1=0;

std::vector<double> j0vec, j1vec, j2vec, j3vec;

opengl::opengl(QWidget *parent)
    : QOpenGLWidget(parent)
{
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(50);
}

opengl::~opengl()
{
    //! destructor
}

void opengl::initializeGL()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_STIPPLE);
    setMouseTracking(true);
}

void opengl::resizeGL(int w, int h)
{
    // std::cout<<"resize"<<std::endl;
    glViewport(0, 0, w, h);
}

void opengl::clearvec(){
    j0vec.clear();
    j1vec.clear();
    j2vec.clear();
    j3vec.clear();
}
void opengl::setScale(double thexScale, double theyScale){
    xScale=thexScale;
    yScale=theyScale;
}

void opengl::setInterval(double theInterval){
    interval=theInterval;
}

void opengl::setj0vec(std::vector<double> theyVec){
    j0vec=theyVec;
}

void opengl::setj1vec(std::vector<double> theyVec){
    j1vec=theyVec;
}

void opengl::setj2vec(std::vector<double> theyVec){
    j2vec=theyVec;
}

void opengl::setj3Vec(std::vector<double> theyVec){
    j3vec=theyVec;
}

void opengl::set2VecShift(double theyShift){
    yShift=theyShift;
}

void opengl::set1VecScale(double theScale){
    scale1=theScale;
}

void opengl::paintGL()
{
    glViewport(0, 0, this->width(), this->height());
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, this->width(), this->height(), 0, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(255,0,0);
    glBegin(GL_LINE_STRIP);
    glLineWidth(1);
    t=0;
    for(uint i=0; i<j0vec.size(); i++){
        glVertex2d(t*xScale,(this->height()-yShift)-(j0vec.at(i)*yScale));
        t+=interval;
    }
    glEnd();

    glColor3f(255,255,0);
    glBegin(GL_LINE_STRIP);
    t=0;
    for(uint i=0; i<j1vec.size(); i++){
        glVertex2d(t*xScale,this->height()-(j1vec.at(i)*scale1));
        t+=interval;
    }
    glEnd();

    glColor3f(255,255,255);
    glBegin(GL_LINE_STRIP);
    t=0;
    for(uint i=0; i<j2vec.size(); i++){
        glVertex2d(t*xScale,(this->height()-yShift)-(j2vec.at(i)*yScale));
        t+=interval;
    }
    glEnd();

    glColor3f(0,255,255);
    glBegin(GL_LINE_STRIP);
    glLineWidth(1);
    t=0;
    for(uint i=0; i<j3vec.size(); i++){
        glVertex2d(t*xScale,this->height()-(j3vec.at(i)*yScale));
        t+=interval;
    }
    glEnd();
    glEnd();

}












