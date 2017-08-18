#include "latencylinechart.h"

LatencyLineChart::LatencyLineChart()
{
	//Temporizador para mediciones
	measure_time.start();

	//Timer para redibujar grafico
	replot_timer.setInterval(1000 / 30);
	replot_timer.setSingleShot(false);
	replot_timer.start();
	connect(&replot_timer, SIGNAL(timeout()), this, SLOT(replot()));

	//Inicializar datos
	for (int i = 0; i < 16; i++)
	{
		addGraph();

		QColor pen_color;
		//pen_color.setHsl(i * 360 / 16, 80, 80);
		pen_color.setHsl(i * 3 * 360 / 16, 200, 150);
		graph(i)->setPen(QPen(pen_color));
	}


	QSharedPointer<QCPAxisTickerTime> ticker(new QCPAxisTickerTime);
	ticker->setTimeFormat("%m:%s");
	xAxis->setTicker(ticker);
	xAxis->setTickLength(2, 2);
	xAxis->setRange(-10.1, -0.1);
	xAxis->grid()->setVisible(true);

	yrange = 1;
	yrange_target = 1;
	yAxis->setRange(0, 1);

	legend->setVisible(false);

	connect(this, SIGNAL(beforeReplot()), this, SLOT(updateData()));
}

void LatencyLineChart::updateLatency(uint value, uint sequence, uint node)
{
	graph(node)->addData(measure_time.elapsed() / 1000.0, (double)value);
	yrange_target = value > yrange_target ? value : yrange_target;
}

void LatencyLineChart::updateData()
{
	double current_time = measure_time.elapsed() / 1000.0;
	xAxis->setRange(current_time - 10.1, current_time - 0.1);

	yrange = yrange * 0.8 + yrange_target * 0.2;
	yAxis->setRange(0, yrange);
}


void LatencyLineChart::setActive(bool active)
{
	setVisible(active);

	if (active)
	{
		replot_timer.start();
		measure_time.restart();
		for (int i = 0; i < 16; i++)
		{
			graph(i)->data()->clear();
		}
	}
	else
	{
		replot_timer.stop();
	}
}
