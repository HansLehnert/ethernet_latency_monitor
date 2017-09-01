#include "latencybarchart.h"

LatencyBarChart::LatencyBarChart()
{
	//Inicializar datos
	QVector<double> ticks;
	QVector<QString> labels;

	last_sequence = 0;

	for (int i = 0; i < 16; i++)
	{
		last_latency.append(QCPBarsData(i, 0));
		last_latency_target.append(QCPBarsData(i, 0));

		node_sequence[i] = 0;

		bars_latency[i] = new QCPBars(xAxis, yAxis);
		bars_latency[i]->setAntialiased(false);
		bars_latency[i]->setName("Latencia");

		QColor brush_color, pen_color;
		pen_color.setHsl(i * 3 * 360 / 16, 240, 50);
		brush_color.setHsl(i * 3 * 360 / 16, 200, 200);
		bars_latency[i]->setPen(QPen(pen_color));
		bars_latency[i]->setBrush(brush_color);

		bars_latency[i]->data()->set(QVector<QCPBarsData>(1, last_latency[i]));

		ticks << i;
		labels << QString::number(i);
	}


	QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
	textTicker->addTicks(ticks, labels);
	xAxis->setTicker(textTicker);
	xAxis->setTickLength(0, 0);
	xAxis->setRange(-1, 16);
	xAxis->grid()->setVisible(true);

	yrange = 1;
	yrange_target = 1;
	yAxis->setRange(0, 1);

	//bars_latency[0]->data()->set(last_latency);

	legend->setVisible(false);

	connect(this, SIGNAL(beforeReplot()), this, SLOT(updateData()));
}

void LatencyBarChart::addData(uint value, uint sequence, uint node)
{
	if (node < 16) {
		last_latency_target[node].value = (double)value;
		node_sequence[node] = sequence;

		last_sequence = sequence;

		yrange_target = value > yrange_target ? value : yrange_target;
	}
}


void LatencyBarChart::updateData() {
	for (int i = 0; i < 16; i++)
	{
		last_latency[i].value = last_latency[i].value * 0.8 +
		                        last_latency_target[i].value * 0.2;
		bars_latency[i]->data()->set(QVector<QCPBarsData>(1, last_latency[i]));

		//Deteccion de nodos desconectados
		//Se considera desconectado si falla la retransmision de 10 paquetes
		if (last_sequence - node_sequence[i] >= 10)
			last_latency_target[i].value = 0;
	}

	yrange = yrange * 0.8 + yrange_target * 0.2;
	yAxis->setRange(0, yrange);
}


void LatencyBarChart::setActive(bool active) {
	setVisible(active);

	if (active) {
		for (int i = 0; i < 16; i++) {
			last_latency[i].value = 0;
			last_latency_target[i].value = 0;
		}
	}
}
