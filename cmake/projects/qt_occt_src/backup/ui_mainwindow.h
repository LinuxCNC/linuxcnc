/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QGridLayout *gridLayout_3;
    QPushButton *pushButton;
    QGridLayout *gridLayout_2;
    QLabel *label_55;
    QLabel *label_56;
    QLabel *label_57;
    QLabel *label_58;
    QLineEdit *lineEdit_j0;
    QLineEdit *lineEdit_j1;
    QLineEdit *lineEdit_j2;
    QSpacerItem *horizontalSpacer;
    QLabel *label_59;
    QLabel *label_60;
    QLabel *label_61;
    QLineEdit *lineEdit_j3;
    QLineEdit *lineEdit_j4;
    QLineEdit *lineEdit_j5;
    QLabel *label_66;
    QLabel *label_67;
    QLabel *label_68;
    QLabel *label_69;
    QLineEdit *lineEdit_cartx;
    QLineEdit *lineEdit_carty;
    QLineEdit *lineEdit_cartz;
    QLabel *label_70;
    QLabel *label_71;
    QLabel *label_72;
    QLabel *label_73;
    QLineEdit *lineEdit_eulerx;
    QLineEdit *lineEdit_eulery;
    QLineEdit *lineEdit_eulerz;
    QLabel *label_77;
    QLabel *label_76;
    QLabel *label_74;
    QLabel *label_75;
    QLineEdit *lineEdit_gcode_x;
    QLineEdit *lineEdit_gcode_y;
    QLineEdit *lineEdit_gcode_z;
    QLabel *label_78;
    QLabel *label_80;
    QLabel *label_79;
    QLineEdit *lineEdit_gcode_euler_x;
    QLineEdit *lineEdit_gcode_euler_y;
    QLineEdit *lineEdit_gcode_euler_z;
    QFrame *frame;
    QGridLayout *gridLayout;
    QGridLayout *gridLayout_opencascade;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(795, 896);
        QPalette palette;
        QBrush brush(QColor(255, 255, 255, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        QBrush brush1(QColor(65, 65, 65, 255));
        brush1.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
        QBrush brush2(QColor(255, 255, 255, 128));
        brush2.setStyle(Qt::SolidPattern);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        QBrush brush3(QColor(190, 190, 190, 255));
        brush3.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
        QBrush brush4(QColor(0, 0, 0, 128));
        brush4.setStyle(Qt::SolidPattern);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        MainWindow->setPalette(palette);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        gridLayout_3 = new QGridLayout(centralwidget);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        pushButton = new QPushButton(centralwidget);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));

        gridLayout_3->addWidget(pushButton, 0, 0, 1, 1);

        gridLayout_2 = new QGridLayout();
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        label_55 = new QLabel(centralwidget);
        label_55->setObjectName(QString::fromUtf8("label_55"));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette1.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette1.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette1.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette1.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette1.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_55->setPalette(palette1);
        QFont font;
        font.setBold(false);
        font.setWeight(50);
        label_55->setFont(font);
        label_55->setStyleSheet(QString::fromUtf8(""));

        gridLayout_2->addWidget(label_55, 0, 0, 1, 1);

        label_56 = new QLabel(centralwidget);
        label_56->setObjectName(QString::fromUtf8("label_56"));
        QPalette palette2;
        palette2.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Text, brush);
        palette2.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette2.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette2.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette2.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette2.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette2.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette2.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette2.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette2.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_56->setPalette(palette2);

        gridLayout_2->addWidget(label_56, 1, 0, 1, 1);

        label_57 = new QLabel(centralwidget);
        label_57->setObjectName(QString::fromUtf8("label_57"));
        QPalette palette3;
        palette3.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette3.setBrush(QPalette::Active, QPalette::Text, brush);
        palette3.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette3.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette3.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette3.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette3.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette3.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette3.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette3.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette3.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette3.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette3.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette3.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette3.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette3.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette3.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette3.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette3.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette3.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_57->setPalette(palette3);

        gridLayout_2->addWidget(label_57, 1, 1, 1, 1);

        label_58 = new QLabel(centralwidget);
        label_58->setObjectName(QString::fromUtf8("label_58"));
        QPalette palette4;
        palette4.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette4.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette4.setBrush(QPalette::Active, QPalette::Text, brush);
        palette4.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette4.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette4.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette4.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette4.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette4.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette4.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette4.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette4.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette4.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette4.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette4.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette4.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette4.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette4.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette4.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette4.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette4.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette4.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette4.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette4.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_58->setPalette(palette4);

        gridLayout_2->addWidget(label_58, 1, 2, 1, 1);

        lineEdit_j0 = new QLineEdit(centralwidget);
        lineEdit_j0->setObjectName(QString::fromUtf8("lineEdit_j0"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(lineEdit_j0->sizePolicy().hasHeightForWidth());
        lineEdit_j0->setSizePolicy(sizePolicy);
        QPalette palette5;
        QBrush brush5(QColor(0, 0, 0, 255));
        brush5.setStyle(Qt::SolidPattern);
        palette5.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        QBrush brush6(QColor(193, 245, 255, 255));
        brush6.setStyle(Qt::SolidPattern);
        palette5.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette5.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette5.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette5.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette5.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush7(QColor(0, 0, 0, 128));
        brush7.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette5.setBrush(QPalette::Active, QPalette::PlaceholderText, brush7);
#endif
        palette5.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette5.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette5.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette5.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette5.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette5.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush8(QColor(0, 0, 0, 128));
        brush8.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette5.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush8);
#endif
        palette5.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette5.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette5.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette5.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette5.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette5.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush9(QColor(0, 0, 0, 128));
        brush9.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette5.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush9);
#endif
        lineEdit_j0->setPalette(palette5);
        lineEdit_j0->setAutoFillBackground(false);
        lineEdit_j0->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_j0, 2, 0, 1, 1);

        lineEdit_j1 = new QLineEdit(centralwidget);
        lineEdit_j1->setObjectName(QString::fromUtf8("lineEdit_j1"));
        sizePolicy.setHeightForWidth(lineEdit_j1->sizePolicy().hasHeightForWidth());
        lineEdit_j1->setSizePolicy(sizePolicy);
        QPalette palette6;
        palette6.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette6.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette6.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette6.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette6.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette6.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush10(QColor(0, 0, 0, 128));
        brush10.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette6.setBrush(QPalette::Active, QPalette::PlaceholderText, brush10);
#endif
        palette6.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette6.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette6.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette6.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette6.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette6.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush11(QColor(0, 0, 0, 128));
        brush11.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette6.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush11);
#endif
        palette6.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette6.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette6.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette6.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette6.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette6.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush12(QColor(0, 0, 0, 128));
        brush12.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette6.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush12);
#endif
        lineEdit_j1->setPalette(palette6);
        lineEdit_j1->setAutoFillBackground(false);
        lineEdit_j1->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_j1, 2, 1, 1, 1);

        lineEdit_j2 = new QLineEdit(centralwidget);
        lineEdit_j2->setObjectName(QString::fromUtf8("lineEdit_j2"));
        sizePolicy.setHeightForWidth(lineEdit_j2->sizePolicy().hasHeightForWidth());
        lineEdit_j2->setSizePolicy(sizePolicy);
        QPalette palette7;
        palette7.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette7.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette7.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette7.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette7.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette7.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush13(QColor(0, 0, 0, 128));
        brush13.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette7.setBrush(QPalette::Active, QPalette::PlaceholderText, brush13);
#endif
        palette7.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette7.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette7.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette7.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette7.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette7.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush14(QColor(0, 0, 0, 128));
        brush14.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette7.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush14);
#endif
        palette7.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette7.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette7.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette7.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette7.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette7.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush15(QColor(0, 0, 0, 128));
        brush15.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette7.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush15);
#endif
        lineEdit_j2->setPalette(palette7);
        lineEdit_j2->setAutoFillBackground(false);
        lineEdit_j2->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_j2, 2, 2, 1, 1);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_2->addItem(horizontalSpacer, 2, 3, 1, 1);

        label_59 = new QLabel(centralwidget);
        label_59->setObjectName(QString::fromUtf8("label_59"));
        QPalette palette8;
        palette8.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette8.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette8.setBrush(QPalette::Active, QPalette::Text, brush);
        palette8.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette8.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette8.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette8.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette8.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette8.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette8.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette8.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette8.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette8.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette8.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette8.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette8.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette8.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette8.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette8.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette8.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette8.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette8.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette8.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette8.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_59->setPalette(palette8);

        gridLayout_2->addWidget(label_59, 3, 0, 1, 1);

        label_60 = new QLabel(centralwidget);
        label_60->setObjectName(QString::fromUtf8("label_60"));
        QPalette palette9;
        palette9.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette9.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette9.setBrush(QPalette::Active, QPalette::Text, brush);
        palette9.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette9.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette9.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette9.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette9.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette9.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette9.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette9.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette9.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette9.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette9.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette9.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette9.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette9.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette9.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette9.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette9.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette9.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette9.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette9.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette9.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_60->setPalette(palette9);

        gridLayout_2->addWidget(label_60, 3, 1, 1, 1);

        label_61 = new QLabel(centralwidget);
        label_61->setObjectName(QString::fromUtf8("label_61"));
        QPalette palette10;
        palette10.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette10.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette10.setBrush(QPalette::Active, QPalette::Text, brush);
        palette10.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette10.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette10.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette10.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette10.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette10.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette10.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette10.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette10.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette10.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette10.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette10.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette10.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette10.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette10.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette10.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette10.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette10.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette10.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette10.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette10.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_61->setPalette(palette10);

        gridLayout_2->addWidget(label_61, 3, 2, 1, 1);

        lineEdit_j3 = new QLineEdit(centralwidget);
        lineEdit_j3->setObjectName(QString::fromUtf8("lineEdit_j3"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(lineEdit_j3->sizePolicy().hasHeightForWidth());
        lineEdit_j3->setSizePolicy(sizePolicy1);
        QPalette palette11;
        palette11.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette11.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette11.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette11.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette11.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette11.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush16(QColor(0, 0, 0, 128));
        brush16.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette11.setBrush(QPalette::Active, QPalette::PlaceholderText, brush16);
#endif
        palette11.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette11.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette11.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette11.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette11.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette11.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush17(QColor(0, 0, 0, 128));
        brush17.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette11.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush17);
#endif
        palette11.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette11.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette11.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette11.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette11.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette11.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush18(QColor(0, 0, 0, 128));
        brush18.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette11.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush18);
#endif
        lineEdit_j3->setPalette(palette11);
        lineEdit_j3->setAutoFillBackground(false);
        lineEdit_j3->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_j3, 4, 0, 1, 1);

        lineEdit_j4 = new QLineEdit(centralwidget);
        lineEdit_j4->setObjectName(QString::fromUtf8("lineEdit_j4"));
        sizePolicy1.setHeightForWidth(lineEdit_j4->sizePolicy().hasHeightForWidth());
        lineEdit_j4->setSizePolicy(sizePolicy1);
        QPalette palette12;
        palette12.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette12.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette12.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette12.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette12.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette12.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush19(QColor(0, 0, 0, 128));
        brush19.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette12.setBrush(QPalette::Active, QPalette::PlaceholderText, brush19);
#endif
        palette12.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette12.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette12.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette12.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette12.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette12.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush20(QColor(0, 0, 0, 128));
        brush20.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette12.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush20);
#endif
        palette12.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette12.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette12.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette12.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette12.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette12.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush21(QColor(0, 0, 0, 128));
        brush21.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette12.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush21);
#endif
        lineEdit_j4->setPalette(palette12);
        lineEdit_j4->setAutoFillBackground(false);
        lineEdit_j4->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_j4, 4, 1, 1, 1);

        lineEdit_j5 = new QLineEdit(centralwidget);
        lineEdit_j5->setObjectName(QString::fromUtf8("lineEdit_j5"));
        sizePolicy1.setHeightForWidth(lineEdit_j5->sizePolicy().hasHeightForWidth());
        lineEdit_j5->setSizePolicy(sizePolicy1);
        QPalette palette13;
        palette13.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette13.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette13.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette13.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette13.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette13.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush22(QColor(0, 0, 0, 128));
        brush22.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette13.setBrush(QPalette::Active, QPalette::PlaceholderText, brush22);
#endif
        palette13.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette13.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette13.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette13.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette13.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette13.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush23(QColor(0, 0, 0, 128));
        brush23.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette13.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush23);
#endif
        palette13.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette13.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette13.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette13.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette13.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette13.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush24(QColor(0, 0, 0, 128));
        brush24.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette13.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush24);
#endif
        lineEdit_j5->setPalette(palette13);
        lineEdit_j5->setAutoFillBackground(false);
        lineEdit_j5->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_j5, 4, 2, 1, 1);

        label_66 = new QLabel(centralwidget);
        label_66->setObjectName(QString::fromUtf8("label_66"));
        QPalette palette14;
        palette14.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette14.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette14.setBrush(QPalette::Active, QPalette::Text, brush);
        palette14.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette14.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette14.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette14.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette14.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette14.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette14.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette14.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette14.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette14.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette14.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette14.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette14.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette14.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette14.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette14.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette14.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette14.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette14.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette14.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette14.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_66->setPalette(palette14);
        label_66->setFont(font);

        gridLayout_2->addWidget(label_66, 5, 0, 1, 1);

        label_67 = new QLabel(centralwidget);
        label_67->setObjectName(QString::fromUtf8("label_67"));
        QPalette palette15;
        palette15.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette15.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette15.setBrush(QPalette::Active, QPalette::Text, brush);
        palette15.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette15.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette15.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette15.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette15.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette15.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette15.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette15.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette15.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette15.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette15.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette15.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette15.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette15.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette15.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette15.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette15.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette15.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette15.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette15.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette15.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_67->setPalette(palette15);

        gridLayout_2->addWidget(label_67, 6, 0, 1, 1);

        label_68 = new QLabel(centralwidget);
        label_68->setObjectName(QString::fromUtf8("label_68"));
        QPalette palette16;
        palette16.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette16.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette16.setBrush(QPalette::Active, QPalette::Text, brush);
        palette16.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette16.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette16.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette16.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette16.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette16.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette16.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette16.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette16.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette16.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette16.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette16.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette16.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette16.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette16.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette16.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette16.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette16.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette16.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette16.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette16.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_68->setPalette(palette16);

        gridLayout_2->addWidget(label_68, 6, 1, 1, 1);

        label_69 = new QLabel(centralwidget);
        label_69->setObjectName(QString::fromUtf8("label_69"));
        QPalette palette17;
        palette17.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette17.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette17.setBrush(QPalette::Active, QPalette::Text, brush);
        palette17.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette17.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette17.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette17.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette17.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette17.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette17.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette17.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette17.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette17.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette17.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette17.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette17.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette17.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette17.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette17.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette17.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette17.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette17.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette17.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette17.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_69->setPalette(palette17);

        gridLayout_2->addWidget(label_69, 6, 2, 1, 1);

        lineEdit_cartx = new QLineEdit(centralwidget);
        lineEdit_cartx->setObjectName(QString::fromUtf8("lineEdit_cartx"));
        sizePolicy1.setHeightForWidth(lineEdit_cartx->sizePolicy().hasHeightForWidth());
        lineEdit_cartx->setSizePolicy(sizePolicy1);
        QPalette palette18;
        palette18.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette18.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette18.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette18.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette18.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette18.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush25(QColor(0, 0, 0, 128));
        brush25.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette18.setBrush(QPalette::Active, QPalette::PlaceholderText, brush25);
#endif
        palette18.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette18.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette18.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette18.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette18.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette18.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush26(QColor(0, 0, 0, 128));
        brush26.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette18.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush26);
#endif
        palette18.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette18.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette18.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette18.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette18.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette18.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush27(QColor(0, 0, 0, 128));
        brush27.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette18.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush27);
#endif
        lineEdit_cartx->setPalette(palette18);
        lineEdit_cartx->setAutoFillBackground(false);
        lineEdit_cartx->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_cartx, 7, 0, 1, 1);

        lineEdit_carty = new QLineEdit(centralwidget);
        lineEdit_carty->setObjectName(QString::fromUtf8("lineEdit_carty"));
        sizePolicy1.setHeightForWidth(lineEdit_carty->sizePolicy().hasHeightForWidth());
        lineEdit_carty->setSizePolicy(sizePolicy1);
        QPalette palette19;
        palette19.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette19.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette19.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette19.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette19.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette19.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush28(QColor(0, 0, 0, 128));
        brush28.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette19.setBrush(QPalette::Active, QPalette::PlaceholderText, brush28);
#endif
        palette19.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette19.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette19.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette19.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette19.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette19.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush29(QColor(0, 0, 0, 128));
        brush29.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette19.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush29);
#endif
        palette19.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette19.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette19.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette19.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette19.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette19.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush30(QColor(0, 0, 0, 128));
        brush30.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette19.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush30);
#endif
        lineEdit_carty->setPalette(palette19);
        lineEdit_carty->setAutoFillBackground(false);
        lineEdit_carty->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_carty, 7, 1, 1, 1);

        lineEdit_cartz = new QLineEdit(centralwidget);
        lineEdit_cartz->setObjectName(QString::fromUtf8("lineEdit_cartz"));
        sizePolicy1.setHeightForWidth(lineEdit_cartz->sizePolicy().hasHeightForWidth());
        lineEdit_cartz->setSizePolicy(sizePolicy1);
        QPalette palette20;
        palette20.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette20.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette20.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette20.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette20.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette20.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush31(QColor(0, 0, 0, 128));
        brush31.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette20.setBrush(QPalette::Active, QPalette::PlaceholderText, brush31);
#endif
        palette20.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette20.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette20.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette20.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette20.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette20.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush32(QColor(0, 0, 0, 128));
        brush32.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette20.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush32);
#endif
        palette20.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette20.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette20.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette20.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette20.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette20.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush33(QColor(0, 0, 0, 128));
        brush33.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette20.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush33);
#endif
        lineEdit_cartz->setPalette(palette20);
        lineEdit_cartz->setAutoFillBackground(false);
        lineEdit_cartz->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_cartz, 7, 2, 1, 1);

        label_70 = new QLabel(centralwidget);
        label_70->setObjectName(QString::fromUtf8("label_70"));
        QPalette palette21;
        palette21.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette21.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette21.setBrush(QPalette::Active, QPalette::Text, brush);
        palette21.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette21.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette21.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette21.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette21.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette21.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette21.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette21.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette21.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette21.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette21.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette21.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette21.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette21.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette21.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette21.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette21.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette21.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette21.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette21.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette21.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_70->setPalette(palette21);
        label_70->setFont(font);

        gridLayout_2->addWidget(label_70, 8, 0, 1, 1);

        label_71 = new QLabel(centralwidget);
        label_71->setObjectName(QString::fromUtf8("label_71"));
        QPalette palette22;
        palette22.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette22.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette22.setBrush(QPalette::Active, QPalette::Text, brush);
        palette22.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette22.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette22.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette22.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette22.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette22.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette22.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette22.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette22.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette22.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette22.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette22.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette22.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette22.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette22.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette22.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette22.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette22.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette22.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette22.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette22.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_71->setPalette(palette22);

        gridLayout_2->addWidget(label_71, 9, 0, 1, 1);

        label_72 = new QLabel(centralwidget);
        label_72->setObjectName(QString::fromUtf8("label_72"));
        QPalette palette23;
        palette23.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette23.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette23.setBrush(QPalette::Active, QPalette::Text, brush);
        palette23.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette23.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette23.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette23.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette23.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette23.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette23.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette23.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette23.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette23.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette23.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette23.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette23.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette23.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette23.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette23.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette23.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette23.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette23.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette23.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette23.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_72->setPalette(palette23);

        gridLayout_2->addWidget(label_72, 9, 1, 1, 1);

        label_73 = new QLabel(centralwidget);
        label_73->setObjectName(QString::fromUtf8("label_73"));
        QPalette palette24;
        palette24.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette24.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette24.setBrush(QPalette::Active, QPalette::Text, brush);
        palette24.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette24.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette24.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette24.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette24.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette24.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette24.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette24.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette24.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette24.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette24.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette24.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette24.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette24.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette24.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette24.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette24.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette24.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette24.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette24.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette24.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_73->setPalette(palette24);

        gridLayout_2->addWidget(label_73, 9, 2, 1, 1);

        lineEdit_eulerx = new QLineEdit(centralwidget);
        lineEdit_eulerx->setObjectName(QString::fromUtf8("lineEdit_eulerx"));
        sizePolicy1.setHeightForWidth(lineEdit_eulerx->sizePolicy().hasHeightForWidth());
        lineEdit_eulerx->setSizePolicy(sizePolicy1);
        QPalette palette25;
        palette25.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette25.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette25.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette25.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette25.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette25.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush34(QColor(0, 0, 0, 128));
        brush34.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette25.setBrush(QPalette::Active, QPalette::PlaceholderText, brush34);
#endif
        palette25.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette25.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette25.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette25.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette25.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette25.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush35(QColor(0, 0, 0, 128));
        brush35.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette25.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush35);
#endif
        palette25.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette25.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette25.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette25.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette25.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette25.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush36(QColor(0, 0, 0, 128));
        brush36.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette25.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush36);
#endif
        lineEdit_eulerx->setPalette(palette25);
        lineEdit_eulerx->setAutoFillBackground(false);
        lineEdit_eulerx->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_eulerx, 10, 0, 1, 1);

        lineEdit_eulery = new QLineEdit(centralwidget);
        lineEdit_eulery->setObjectName(QString::fromUtf8("lineEdit_eulery"));
        sizePolicy1.setHeightForWidth(lineEdit_eulery->sizePolicy().hasHeightForWidth());
        lineEdit_eulery->setSizePolicy(sizePolicy1);
        QPalette palette26;
        palette26.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette26.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette26.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette26.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette26.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette26.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush37(QColor(0, 0, 0, 128));
        brush37.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette26.setBrush(QPalette::Active, QPalette::PlaceholderText, brush37);
#endif
        palette26.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette26.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette26.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette26.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette26.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette26.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush38(QColor(0, 0, 0, 128));
        brush38.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette26.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush38);
#endif
        palette26.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette26.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette26.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette26.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette26.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette26.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush39(QColor(0, 0, 0, 128));
        brush39.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette26.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush39);
#endif
        lineEdit_eulery->setPalette(palette26);
        lineEdit_eulery->setAutoFillBackground(false);
        lineEdit_eulery->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_eulery, 10, 1, 1, 1);

        lineEdit_eulerz = new QLineEdit(centralwidget);
        lineEdit_eulerz->setObjectName(QString::fromUtf8("lineEdit_eulerz"));
        sizePolicy1.setHeightForWidth(lineEdit_eulerz->sizePolicy().hasHeightForWidth());
        lineEdit_eulerz->setSizePolicy(sizePolicy1);
        QPalette palette27;
        palette27.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette27.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette27.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette27.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette27.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette27.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush40(QColor(0, 0, 0, 128));
        brush40.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette27.setBrush(QPalette::Active, QPalette::PlaceholderText, brush40);
#endif
        palette27.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette27.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette27.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette27.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette27.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette27.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush41(QColor(0, 0, 0, 128));
        brush41.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette27.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush41);
#endif
        palette27.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette27.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette27.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette27.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette27.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette27.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush42(QColor(0, 0, 0, 128));
        brush42.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette27.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush42);
#endif
        lineEdit_eulerz->setPalette(palette27);
        lineEdit_eulerz->setAutoFillBackground(false);
        lineEdit_eulerz->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_eulerz, 10, 2, 1, 1);

        label_77 = new QLabel(centralwidget);
        label_77->setObjectName(QString::fromUtf8("label_77"));
        QPalette palette28;
        palette28.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette28.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette28.setBrush(QPalette::Active, QPalette::Text, brush);
        palette28.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette28.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette28.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette28.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette28.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette28.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette28.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette28.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette28.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette28.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette28.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette28.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette28.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette28.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette28.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette28.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette28.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette28.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette28.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette28.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette28.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_77->setPalette(palette28);
        label_77->setFont(font);

        gridLayout_2->addWidget(label_77, 11, 0, 1, 3);

        label_76 = new QLabel(centralwidget);
        label_76->setObjectName(QString::fromUtf8("label_76"));
        QPalette palette29;
        palette29.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette29.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette29.setBrush(QPalette::Active, QPalette::Text, brush);
        palette29.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette29.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette29.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette29.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette29.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette29.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette29.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette29.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette29.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette29.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette29.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette29.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette29.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette29.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette29.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette29.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette29.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette29.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette29.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette29.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette29.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_76->setPalette(palette29);

        gridLayout_2->addWidget(label_76, 12, 0, 1, 1);

        label_74 = new QLabel(centralwidget);
        label_74->setObjectName(QString::fromUtf8("label_74"));
        QPalette palette30;
        palette30.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette30.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette30.setBrush(QPalette::Active, QPalette::Text, brush);
        palette30.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette30.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette30.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette30.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette30.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette30.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette30.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette30.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette30.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette30.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette30.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette30.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette30.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette30.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette30.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette30.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette30.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette30.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette30.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette30.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette30.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_74->setPalette(palette30);

        gridLayout_2->addWidget(label_74, 12, 1, 1, 1);

        label_75 = new QLabel(centralwidget);
        label_75->setObjectName(QString::fromUtf8("label_75"));
        QPalette palette31;
        palette31.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette31.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette31.setBrush(QPalette::Active, QPalette::Text, brush);
        palette31.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette31.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette31.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette31.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette31.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette31.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette31.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette31.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette31.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette31.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette31.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette31.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette31.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette31.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette31.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette31.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette31.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette31.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette31.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette31.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette31.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_75->setPalette(palette31);

        gridLayout_2->addWidget(label_75, 12, 2, 1, 1);

        lineEdit_gcode_x = new QLineEdit(centralwidget);
        lineEdit_gcode_x->setObjectName(QString::fromUtf8("lineEdit_gcode_x"));
        sizePolicy1.setHeightForWidth(lineEdit_gcode_x->sizePolicy().hasHeightForWidth());
        lineEdit_gcode_x->setSizePolicy(sizePolicy1);
        QPalette palette32;
        palette32.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette32.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette32.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette32.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette32.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette32.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush43(QColor(0, 0, 0, 128));
        brush43.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette32.setBrush(QPalette::Active, QPalette::PlaceholderText, brush43);
#endif
        palette32.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette32.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette32.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette32.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette32.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette32.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush44(QColor(0, 0, 0, 128));
        brush44.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette32.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush44);
#endif
        palette32.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette32.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette32.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette32.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette32.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette32.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush45(QColor(0, 0, 0, 128));
        brush45.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette32.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush45);
#endif
        lineEdit_gcode_x->setPalette(palette32);
        lineEdit_gcode_x->setAutoFillBackground(false);
        lineEdit_gcode_x->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_gcode_x, 13, 0, 1, 1);

        lineEdit_gcode_y = new QLineEdit(centralwidget);
        lineEdit_gcode_y->setObjectName(QString::fromUtf8("lineEdit_gcode_y"));
        sizePolicy1.setHeightForWidth(lineEdit_gcode_y->sizePolicy().hasHeightForWidth());
        lineEdit_gcode_y->setSizePolicy(sizePolicy1);
        QPalette palette33;
        palette33.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette33.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette33.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette33.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette33.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette33.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush46(QColor(0, 0, 0, 128));
        brush46.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette33.setBrush(QPalette::Active, QPalette::PlaceholderText, brush46);
#endif
        palette33.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette33.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette33.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette33.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette33.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette33.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush47(QColor(0, 0, 0, 128));
        brush47.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette33.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush47);
#endif
        palette33.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette33.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette33.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette33.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette33.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette33.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush48(QColor(0, 0, 0, 128));
        brush48.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette33.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush48);
#endif
        lineEdit_gcode_y->setPalette(palette33);
        lineEdit_gcode_y->setAutoFillBackground(false);
        lineEdit_gcode_y->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_gcode_y, 13, 1, 1, 1);

        lineEdit_gcode_z = new QLineEdit(centralwidget);
        lineEdit_gcode_z->setObjectName(QString::fromUtf8("lineEdit_gcode_z"));
        sizePolicy1.setHeightForWidth(lineEdit_gcode_z->sizePolicy().hasHeightForWidth());
        lineEdit_gcode_z->setSizePolicy(sizePolicy1);
        QPalette palette34;
        palette34.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette34.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette34.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette34.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette34.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette34.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush49(QColor(0, 0, 0, 128));
        brush49.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette34.setBrush(QPalette::Active, QPalette::PlaceholderText, brush49);
#endif
        palette34.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette34.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette34.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette34.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette34.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette34.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush50(QColor(0, 0, 0, 128));
        brush50.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette34.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush50);
#endif
        palette34.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette34.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette34.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette34.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette34.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette34.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush51(QColor(0, 0, 0, 128));
        brush51.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette34.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush51);
#endif
        lineEdit_gcode_z->setPalette(palette34);
        lineEdit_gcode_z->setAutoFillBackground(false);
        lineEdit_gcode_z->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_gcode_z, 13, 2, 1, 1);

        label_78 = new QLabel(centralwidget);
        label_78->setObjectName(QString::fromUtf8("label_78"));
        QPalette palette35;
        palette35.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette35.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette35.setBrush(QPalette::Active, QPalette::Text, brush);
        palette35.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette35.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette35.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette35.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette35.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette35.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette35.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette35.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette35.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette35.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette35.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette35.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette35.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette35.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette35.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette35.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette35.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette35.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette35.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette35.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette35.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_78->setPalette(palette35);

        gridLayout_2->addWidget(label_78, 14, 0, 1, 1);

        label_80 = new QLabel(centralwidget);
        label_80->setObjectName(QString::fromUtf8("label_80"));
        QPalette palette36;
        palette36.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette36.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette36.setBrush(QPalette::Active, QPalette::Text, brush);
        palette36.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette36.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette36.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette36.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette36.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette36.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette36.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette36.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette36.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette36.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette36.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette36.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette36.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette36.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette36.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette36.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette36.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette36.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette36.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette36.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette36.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_80->setPalette(palette36);

        gridLayout_2->addWidget(label_80, 14, 1, 1, 1);

        label_79 = new QLabel(centralwidget);
        label_79->setObjectName(QString::fromUtf8("label_79"));
        QPalette palette37;
        palette37.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette37.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette37.setBrush(QPalette::Active, QPalette::Text, brush);
        palette37.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette37.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette37.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette37.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette37.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette37.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette37.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette37.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette37.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette37.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette37.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette37.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette37.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette37.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette37.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette37.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette37.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette37.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette37.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette37.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette37.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        label_79->setPalette(palette37);

        gridLayout_2->addWidget(label_79, 14, 2, 1, 1);

        lineEdit_gcode_euler_x = new QLineEdit(centralwidget);
        lineEdit_gcode_euler_x->setObjectName(QString::fromUtf8("lineEdit_gcode_euler_x"));
        sizePolicy1.setHeightForWidth(lineEdit_gcode_euler_x->sizePolicy().hasHeightForWidth());
        lineEdit_gcode_euler_x->setSizePolicy(sizePolicy1);
        QPalette palette38;
        palette38.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette38.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette38.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette38.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette38.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette38.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush52(QColor(0, 0, 0, 128));
        brush52.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette38.setBrush(QPalette::Active, QPalette::PlaceholderText, brush52);
#endif
        palette38.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette38.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette38.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette38.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette38.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette38.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush53(QColor(0, 0, 0, 128));
        brush53.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette38.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush53);
#endif
        palette38.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette38.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette38.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette38.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette38.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette38.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush54(QColor(0, 0, 0, 128));
        brush54.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette38.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush54);
#endif
        lineEdit_gcode_euler_x->setPalette(palette38);
        lineEdit_gcode_euler_x->setAutoFillBackground(false);
        lineEdit_gcode_euler_x->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_gcode_euler_x, 15, 0, 1, 1);

        lineEdit_gcode_euler_y = new QLineEdit(centralwidget);
        lineEdit_gcode_euler_y->setObjectName(QString::fromUtf8("lineEdit_gcode_euler_y"));
        sizePolicy1.setHeightForWidth(lineEdit_gcode_euler_y->sizePolicy().hasHeightForWidth());
        lineEdit_gcode_euler_y->setSizePolicy(sizePolicy1);
        QPalette palette39;
        palette39.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette39.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette39.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette39.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette39.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette39.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush55(QColor(0, 0, 0, 128));
        brush55.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette39.setBrush(QPalette::Active, QPalette::PlaceholderText, brush55);
#endif
        palette39.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette39.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette39.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette39.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette39.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette39.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush56(QColor(0, 0, 0, 128));
        brush56.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette39.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush56);
#endif
        palette39.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette39.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette39.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette39.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette39.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette39.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush57(QColor(0, 0, 0, 128));
        brush57.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette39.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush57);
#endif
        lineEdit_gcode_euler_y->setPalette(palette39);
        lineEdit_gcode_euler_y->setAutoFillBackground(false);
        lineEdit_gcode_euler_y->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_gcode_euler_y, 15, 1, 1, 1);

        lineEdit_gcode_euler_z = new QLineEdit(centralwidget);
        lineEdit_gcode_euler_z->setObjectName(QString::fromUtf8("lineEdit_gcode_euler_z"));
        sizePolicy1.setHeightForWidth(lineEdit_gcode_euler_z->sizePolicy().hasHeightForWidth());
        lineEdit_gcode_euler_z->setSizePolicy(sizePolicy1);
        QPalette palette40;
        palette40.setBrush(QPalette::Active, QPalette::WindowText, brush5);
        palette40.setBrush(QPalette::Active, QPalette::Button, brush6);
        palette40.setBrush(QPalette::Active, QPalette::Text, brush5);
        palette40.setBrush(QPalette::Active, QPalette::ButtonText, brush5);
        palette40.setBrush(QPalette::Active, QPalette::Base, brush6);
        palette40.setBrush(QPalette::Active, QPalette::Window, brush6);
        QBrush brush58(QColor(0, 0, 0, 128));
        brush58.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette40.setBrush(QPalette::Active, QPalette::PlaceholderText, brush58);
#endif
        palette40.setBrush(QPalette::Inactive, QPalette::WindowText, brush5);
        palette40.setBrush(QPalette::Inactive, QPalette::Button, brush6);
        palette40.setBrush(QPalette::Inactive, QPalette::Text, brush5);
        palette40.setBrush(QPalette::Inactive, QPalette::ButtonText, brush5);
        palette40.setBrush(QPalette::Inactive, QPalette::Base, brush6);
        palette40.setBrush(QPalette::Inactive, QPalette::Window, brush6);
        QBrush brush59(QColor(0, 0, 0, 128));
        brush59.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette40.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush59);
#endif
        palette40.setBrush(QPalette::Disabled, QPalette::WindowText, brush5);
        palette40.setBrush(QPalette::Disabled, QPalette::Button, brush6);
        palette40.setBrush(QPalette::Disabled, QPalette::Text, brush5);
        palette40.setBrush(QPalette::Disabled, QPalette::ButtonText, brush5);
        palette40.setBrush(QPalette::Disabled, QPalette::Base, brush6);
        palette40.setBrush(QPalette::Disabled, QPalette::Window, brush6);
        QBrush brush60(QColor(0, 0, 0, 128));
        brush60.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette40.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush60);
#endif
        lineEdit_gcode_euler_z->setPalette(palette40);
        lineEdit_gcode_euler_z->setAutoFillBackground(false);
        lineEdit_gcode_euler_z->setStyleSheet(QString::fromUtf8("background-color: rgb(193, 245, 255);\n"
"color: rgb(0, 0, 0);"));

        gridLayout_2->addWidget(lineEdit_gcode_euler_z, 15, 2, 1, 1);


        gridLayout_3->addLayout(gridLayout_2, 1, 0, 1, 1);

        frame = new QFrame(centralwidget);
        frame->setObjectName(QString::fromUtf8("frame"));
        QSizePolicy sizePolicy2(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(frame->sizePolicy().hasHeightForWidth());
        frame->setSizePolicy(sizePolicy2);
        QPalette palette41;
        palette41.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette41.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette41.setBrush(QPalette::Active, QPalette::Text, brush);
        palette41.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette41.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette41.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette41.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette41.setBrush(QPalette::Active, QPalette::PlaceholderText, brush2);
#endif
        palette41.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette41.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette41.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette41.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette41.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette41.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette41.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette41.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette41.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
        palette41.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette41.setBrush(QPalette::Disabled, QPalette::Text, brush3);
        palette41.setBrush(QPalette::Disabled, QPalette::ButtonText, brush3);
        palette41.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette41.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette41.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette41.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush4);
#endif
        frame->setPalette(palette41);
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        gridLayout = new QGridLayout(frame);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout_opencascade = new QGridLayout();
        gridLayout_opencascade->setObjectName(QString::fromUtf8("gridLayout_opencascade"));

        gridLayout->addLayout(gridLayout_opencascade, 0, 0, 1, 1);


        gridLayout_3->addWidget(frame, 2, 0, 1, 1);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 795, 20));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        pushButton->setText(QCoreApplication::translate("MainWindow", "gcode", nullptr));
        label_55->setText(QCoreApplication::translate("MainWindow", "Joints", nullptr));
        label_56->setText(QCoreApplication::translate("MainWindow", "J0", nullptr));
        label_57->setText(QCoreApplication::translate("MainWindow", "J1", nullptr));
        label_58->setText(QCoreApplication::translate("MainWindow", "J2", nullptr));
        lineEdit_j0->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_j1->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_j2->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        label_59->setText(QCoreApplication::translate("MainWindow", "J3", nullptr));
        label_60->setText(QCoreApplication::translate("MainWindow", "J4", nullptr));
        label_61->setText(QCoreApplication::translate("MainWindow", "J5", nullptr));
        lineEdit_j3->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_j4->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_j5->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        label_66->setText(QCoreApplication::translate("MainWindow", "Cart", nullptr));
        label_67->setText(QCoreApplication::translate("MainWindow", "X", nullptr));
        label_68->setText(QCoreApplication::translate("MainWindow", "Y", nullptr));
        label_69->setText(QCoreApplication::translate("MainWindow", "Z", nullptr));
        lineEdit_cartx->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_carty->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_cartz->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        label_70->setText(QCoreApplication::translate("MainWindow", "Euler ", nullptr));
        label_71->setText(QCoreApplication::translate("MainWindow", "X", nullptr));
        label_72->setText(QCoreApplication::translate("MainWindow", "Y", nullptr));
        label_73->setText(QCoreApplication::translate("MainWindow", "Z", nullptr));
        lineEdit_eulerx->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_eulery->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_eulerz->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        label_77->setText(QCoreApplication::translate("MainWindow", "Gcode quaternion (rotate & translate gcode)", nullptr));
        label_76->setText(QCoreApplication::translate("MainWindow", "X", nullptr));
        label_74->setText(QCoreApplication::translate("MainWindow", "Y", nullptr));
        label_75->setText(QCoreApplication::translate("MainWindow", "Z", nullptr));
        lineEdit_gcode_x->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_gcode_y->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_gcode_z->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        label_78->setText(QCoreApplication::translate("MainWindow", "Euler X (Radians?)", nullptr));
        label_80->setText(QCoreApplication::translate("MainWindow", "Euler Y", nullptr));
        label_79->setText(QCoreApplication::translate("MainWindow", "Euler Z ", nullptr));
        lineEdit_gcode_euler_x->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_gcode_euler_y->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
        lineEdit_gcode_euler_z->setText(QCoreApplication::translate("MainWindow", "0.000", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
