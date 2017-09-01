#include "bandwidthmonitor.h"

#include <QDebug>

#include "serialize.h"

BandwidthMonitor::BandwidthMonitor(QObject *parent) : QObject(parent) {
	first_value_in = false;
	first_value_out = false;
	last_bytes_in = 0;
	last_bytes_out = 0;

	time.start();

	measure_timer.setInterval(1000);

	connect(&measure_timer, &QTimer::timeout,
	        [=] () {
		emit requestMeasurement(NetworkNode::CMD_GET_BYTES_IN, {}, 8);
		emit requestMeasurement(NetworkNode::CMD_GET_BYTES_OUT, {}, 8);
	});
}


void BandwidthMonitor::start() {
	time.restart();

	measure_timer.start();

	first_value_in = false;
	first_value_out = false;
}


void BandwidthMonitor::stop() {
	measure_timer.stop();
}


void BandwidthMonitor::getMeasurement(NetworkNode::Command command, QVector<uchar> response) {
	if (command == NetworkNode::CMD_GET_BYTES_IN) {
		uint64_t bits = deserialize<uint64_t>(response);

		int elapsed_time = time.elapsed();
		if (first_value_in) {
			double bandwidth = (bits - last_bytes_in) * 8
			                   / (double)(elapsed_time - last_elapsed_in);
			emit inMeasurement(bandwidth);
		}
		else {
			first_value_in = true;
		}

		last_bytes_in = bits;
		last_elapsed_in = elapsed_time;
	}
	else if (command == NetworkNode::CMD_GET_BYTES_OUT)  {
		uint64_t bits = deserialize<uint64_t>(response);

		int elapsed_time = time.elapsed();
		if (first_value_out) {
			double bandwidth = (bits - last_bytes_out) * 8
			                   / (double)(elapsed_time - last_elapsed_out);
			emit outMeasurement(bandwidth);
		}
		else {
			first_value_out = true;
		}

		last_bytes_out = bits;
		last_elapsed_out = elapsed_time;
	}
}
