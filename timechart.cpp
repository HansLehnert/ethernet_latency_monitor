#include "timechart.h"

TimeChart::TimeChart(int channels) {
	//Temporizador para mediciones
	measure_time.start();

	setDataChannels(channels);

	QSharedPointer<QCPAxisTickerTime> ticker(new QCPAxisTickerTime);
	ticker->setTimeFormat("%m:%s");
	xAxis->setTicker(ticker);
	xAxis->setTickLength(2, 2);
	xAxis->setRange(-10.1, -0.1);
	xAxis->grid()->setVisible(true);

	y_range[0] = 0;
	y_range[1] = 10;
	y_range_target = 10;
	yAxis->setRange(0, 1);

	legend->setVisible(false);
}


void TimeChart::setDataChannels(int channels) {
	n_channels = channels;

	//Crear graficos
	clearGraphs();

	for (int i = 0; i < n_channels; i++)
	{
		addGraph();

		QColor pen_color;
		pen_color.setHsl(i * 360 / n_channels, 200, 150);
		//pen_color.setHsl(i * 3 * 360 / 16, 200, 150);
		graph(i)->setPen(QPen(pen_color));
	}

	connect(this, SIGNAL(beforeReplot()), this, SLOT(updateData()));
}


void TimeChart::addData(uint channel, double value) {
	graph(channel)->addData(measure_time.elapsed() / 1000.0, value);
}


void TimeChart::updateData()
{
	//Actualizar datos de los gráficos
	double max_value = 0;
	for (int i = 0; i < n_channels; i++) {
		QSharedPointer<QCPGraphDataContainer> graph_data = graph(i)->data();

		//Remover datos fuera del rango inferior
		graph_data->removeBefore(x_range[0] - 1);

		//Encontrar el valor máximo
		int n_data = graph_data->size();
		for (int j = 0; j < n_data; j++) {
			double value = graph_data->at(j)->value;
			if (value > max_value)
				max_value = value;
		}
	}

	//Actualizar rangos
	double current_time = measure_time.elapsed() / 1000.0;

	x_range[0] = current_time - 10.1;
	x_range[1] = current_time - 0.1;

	xAxis->setRange(x_range[0], x_range[1]);

	y_range_target = 10;
	bool flip = true;
	while (y_range_target < max_value) {
		if (flip)
			y_range_target *= 5;
		else
			y_range_target *= 2;
		flip = !flip;
	}

	y_range[1] = 0.8 * y_range[1] + 0.2 * y_range_target; //Animación simple
	if (y_range_target - y_range[1] < 0.01 * y_range_target)
		y_range[1] = y_range_target;
	yAxis->setRange(y_range[0], y_range[1]);
}
