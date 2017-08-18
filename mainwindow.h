#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "serialmanager.h"
#include "latencybarchart.h"
#include "latencylinechart.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

private:
	Ui::MainWindow *ui;

	SerialManager* serial_thread;

	//Graficos
	LatencyBarChart* latency_bar_chart;
	LatencyLineChart* latency_line_chart;

	//Datos del dispositivo
	uint device_precision = 8;

	//Logger
	QTime log_time;
	bool log_status = false;

	struct Measurement {
		double timestamp;
		int node;
		int value;
		int sequence;
	};
	QVector<Measurement> log_data;

signals:
	void requestDeviceInfo();

public slots:
	void scanPorts();
	void connectPort();
	void portStatusChanged(bool);
	void deviceInfoUpdated(quint32, QVector<uchar>);

	void updatePacketSize(int);
	void updatePacketInterval(int);
	void updatePacketDst();
	void updatePacketContent();
	void loadPacketData();

	void controlLogger();

	void selectChart(int);

	void measurementReceived(uint, uint, uint);
};

#endif // MAINWINDOW_H
