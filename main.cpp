#include "mainwindow.h"
#include <QApplication>

#include "networknode.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MainWindow w;
	w.show();

	qRegisterMetaType<QVector<uchar>>("QVector<uchar>");
	qRegisterMetaType<NetworkNode::Command>("NetworkNode::Command");

	return a.exec();
}
