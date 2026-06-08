// main.cpp — 程序入口：创建 QApplication 并显示主窗口

#include "QtProject_1.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QtProject_1 w;
    w.show();
    return app.exec();
}
