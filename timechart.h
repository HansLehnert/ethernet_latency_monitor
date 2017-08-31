#ifndef TIMECHART_H
#define TIMECHART_H

#include "qcustomplot/qcustomplot.h"

class TimeChart : public QCustomPlot
{
	Q_OBJECT

public:
	TimeChart(int);

private:
	QTime measure_time;

	int n_channels;

	double x_range[2];
	double y_range[2];
	double y_range_target;

	void setDataChannels(int);

public slots:
	void addData(uint, double);
	void updateData();
};

#endif // TIMECHART_H
