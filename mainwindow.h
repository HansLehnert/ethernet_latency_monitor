#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QTimer>

#include "serialmanager.h"
#include "bandwidthmonitor.h"
#include "latencybarchart.h"
#include "timechart.h"

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

	SerialManager serial_manager;
	QThread* serial_thread;

	//Timers
	QTimer device_info_timer;
	QTimer replot_timer;

	//Graficos
	LatencyBarChart* latency_bar_chart;
	TimeChart* latency_time_chart;
	TimeChart* bandwidth_time_chart;

	//Datos del dispositivo
	uint device_precision = 8;

	//Monitor de ancho de banda
	BandwidthMonitor bandwidth_monitor;

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

	//Perdida de paquetes
	long last_seq = -1;
	long lost_packets = 0;

public slots:
	void scanPorts();
	void connectPort();
	void portStatusChanged(bool);
	void deviceResponseReceived(NetworkNode::Command, QVector<uchar>);

	void updatePacketDst();
	void updatePacketContent();
	void loadPacketData();

	void requestDeviceInfo();

	void controlLogger();

	void selectChart(int);

	void measurementReceived(uint, uint, uint);
};

#endif // MAINWINDOW_H
