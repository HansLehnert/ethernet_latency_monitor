#ifndef LATENCYBARCHART_H
#define LATENCYBARCHART_H

#include "qcustomplot/qcustomplot.h"

class LatencyBarChart : public QCustomPlot
{
	Q_OBJECT

public:
	LatencyBarChart();

private:
	QTimer replot_timer;

	QCPBars* bars_latency[16];

	QVector<QCPBarsData> last_latency;
	QVector<QCPBarsData> last_latency_target;

	//Usadas para detectar desconexion de los nodos
	uint last_sequence;
	uint node_sequence[16];

	float yrange;
	float yrange_target;

public slots:
	void updateLatency(uint, uint, uint);
	void updateData();
	void setActive(bool);
};

#endif // LATENCYCHART_H
