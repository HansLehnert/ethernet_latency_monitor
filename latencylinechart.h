#ifndef LATENCYLINECHART_H
#define LATENCYLINECHART_H

#include "qcustomplot/qcustomplot.h"

class LatencyLineChart : public QCustomPlot
{
	Q_OBJECT

public:
	LatencyLineChart();

private:
	QTime measure_time;
	QTimer replot_timer;

	float yrange;
	float yrange_target;

public slots:
	void updateLatency(uint, uint, uint);
	void updateData();
	void setActive(bool);
};

#endif // LATENCYCHART_H
