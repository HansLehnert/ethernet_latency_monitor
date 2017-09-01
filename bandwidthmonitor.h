#ifndef BANDWIDTHMONITOR_H
#define BANDWIDTHMONITOR_H

#include <QTime>
#include <QTimer>

#include <cstdint>

#include "networknode.h"

class BandwidthMonitor : public QObject {
	Q_OBJECT
public:
	explicit BandwidthMonitor(QObject *parent = nullptr);

private:
	QTime time;
	int last_elapsed_in;
	int last_elapsed_out;

	QTimer measure_timer;

	uint64_t last_bytes_in;
	uint64_t last_bytes_out;

	bool first_value_in;
	bool first_value_out;

signals:
	void requestMeasurement(NetworkNode::Command, QVector<uchar>, uint);

	void inMeasurement(double);
	void outMeasurement(double);

public slots:
	void start();
	void stop();

	void getMeasurement(NetworkNode::Command command, QVector<uchar> response);
};

#endif // BANDWIDTHMONITOR_H
