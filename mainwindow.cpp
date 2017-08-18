#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>

#include "networknode.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
	ui->setupUi(this);

	serial_thread = new SerialManager();

	//Graficos
	latency_bar_chart = new LatencyBarChart();
	latency_line_chart = new LatencyLineChart();

	ui->verticalLayout_chart->addWidget(latency_line_chart);
	ui->verticalLayout_chart->addWidget(latency_bar_chart);

	ui->comboBox_chartType->addItem("Barras");
	ui->comboBox_chartType->addItem("Líneas");

	selectChart(ui->comboBox_chartType->currentIndex());

	//Conexiones
	connect(ui->pushButton_scan, SIGNAL(clicked(bool)), this, SLOT(scanPorts()));
	connect(ui->pushButton_connect, SIGNAL(clicked(bool)), this, SLOT(connectPort()));
	connect(ui->pushButton_log, SIGNAL(clicked(bool)), this, SLOT(controlLogger()));

	connect(serial_thread, SIGNAL(connected(bool)), this, SLOT(portStatusChanged(bool)));
	connect(serial_thread, SIGNAL(disconnected(bool)), this, SLOT(portStatusChanged(bool)));

	connect(this, SIGNAL(requestDeviceInfo()), serial_thread, SLOT(scheduleCheckDevice()));
	connect(serial_thread, SIGNAL(deviceRegisterUpdated(quint32, QVector<uchar>)), this, SLOT(deviceInfoUpdated(quint32, QVector<uchar>)));

	connect(ui->spinBox_packetSize, SIGNAL(valueChanged(int)), this, SLOT(updatePacketSize(int)));
	connect(ui->spinBox_packetInterval, SIGNAL(valueChanged(int)), this, SLOT(updatePacketInterval(int)));
	connect(ui->spinBox_dst, SIGNAL(valueChanged(int)), this, SLOT(updatePacketDst()));
	connect(ui->checkBox_broadcast, SIGNAL(stateChanged(int)), this, SLOT(updatePacketDst()));
	connect(ui->checkBox_content, SIGNAL(stateChanged(int)), this, SLOT(updatePacketContent()));
	connect(ui->pushButton_content, SIGNAL(clicked(bool)), this, SLOT(loadPacketData()));

	connect(ui->comboBox_chartType, SIGNAL(currentIndexChanged(int)), this, SLOT(selectChart(int)));

	connect(serial_thread, SIGNAL(newMeasurement(uint,uint,uint)), this, SLOT(measurementReceived(uint,uint,uint)));
}

MainWindow::~MainWindow()
{
	delete ui;
}

//Revisa los puertos seriales disponibles en el equipo al presionar el boton correspondiente
void MainWindow::scanPorts()
{
	QList<QSerialPortInfo> port_list = QSerialPortInfo::availablePorts();

	ui->comboBox_port->clear();
	for (QSerialPortInfo &port : port_list) {
		ui->comboBox_port->addItem(port.portName());
	}
}

//Señala la apertura del puerto serial al hilo correspondiente
void MainWindow::connectPort()
{
	QString port_name = ui->comboBox_port->currentText();
	quint32 baudrate = ui->lineEdit_baudrate->text().toInt();
	serial_thread->openPort(port_name, baudrate);
}

//Habilita/deshabilita widgets dependiendo del estado de conexion con el dispositivo
void MainWindow::portStatusChanged(bool connected)
{
	ui->pushButton_scan->setEnabled(!connected);
	ui->comboBox_port->setEnabled(!connected);
	ui->label_id_right->setEnabled(connected);
	ui->label_mac_right->setEnabled(connected);
	ui->label_precision_right->setEnabled(connected);
	ui->label_version_right->setEnabled(connected);
	ui->spinBox_dst->setEnabled(connected);
	ui->spinBox_packetInterval->setEnabled(connected);
	ui->spinBox_packetSize->setEnabled(connected);
	ui->checkBox_broadcast->setEnabled(connected);
	ui->checkBox_content->setEnabled(connected);

	if (connected)
	{
		ui->pushButton_connect->setText("Desconectar");

		//Pedir informacion del dispositivo para actualizar interfaz
		emit requestDeviceInfo();
	}
	else
	{
		ui->pushButton_connect->setText("Conectar");
	}
}

//Actulaizar interfaz al recibir lecturas de los registros del dispositivo
void MainWindow::deviceInfoUpdated(quint32 device_register, QVector<uchar> data)
{
	QString text;
	qint64 value;

	switch (device_register) {
	case NetworkNode::REGISTER_ID:
		text = QString::number(data[0]);
		ui->label_id_right->setText(text);
		break;

	case NetworkNode::REGISTER_VERSION:
		text = QString::number(data[0]) + "." +
		       QString::number(data[1]);
		ui->label_version_right->setText(text);
		break;

	case NetworkNode::REGISTER_PRECISION:
		value = (data[0] << 24) +
		        (data[1] << 16) +
		        (data[2] << 8) +
		        data[3];
		device_precision = value != 0 ? 1000000000 / value : -1;
		text = QString::number(device_precision) + " ns";
		ui->label_precision_right->setText(text);
		break;

	case NetworkNode::REGISTER_MAC_ADDRESS:
		text = QString("%1").arg(data[0], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[1], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[2], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[3], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[4], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[5], 2, 16, QChar('0'));
		ui->label_mac_right->setText(text);
		break;

	case NetworkNode::REGISTER_PACKET_SIZE:
		value = (data[0] << 8) + data[1];
		ui->spinBox_packetSize->setValue(value);
		break;

	case NetworkNode::REGISTER_PACKET_INTERVAL:
		value = (data[0] << 8) + data[1];
		ui->spinBox_packetInterval->setValue(value);
		break;

	case NetworkNode::REGISTER_PACKET_DST:
		if (data[0] == 0xFF)
		{
			ui->checkBox_broadcast->setChecked(true);
		}
		else
		{
			ui->checkBox_broadcast->setChecked(false);
		}
		break;

	case NetworkNode::REGISTER_CUSTOM_PACKET:
		ui->checkBox_content->setChecked(data[0] == 0x01);
	}
}

//Reconfigurar dispositivo con nuevo largo de paquete
void MainWindow::updatePacketSize(int new_value)
{
	unsigned char data[2];
	data[0] = ((quint32) new_value) >> 8;
	data[1] = ((quint32) new_value);

	serial_thread->queueCommand(NetworkNode::CMD_SET_PACKET_SIZE, (char*) data, 2);
}

//Reconfigurar dispositivo con nuevo intervalo entre paquetes
void MainWindow::updatePacketInterval(int new_value)
{
	unsigned char data[2];
	data[0] = ((quint32) new_value) >> 8;
	data[1] = ((quint32) new_value);

	serial_thread->queueCommand(NetworkNode::CMD_SET_PACKET_INTERVAL, (char*) data, 2);
}

//Reconfigurar dispositivo con nuevo destino de paquetes
//Habilita/deshabilita widgets en caso de seleccionar opcion 'broadcast'
void MainWindow::updatePacketDst()
{
	unsigned char dst;

	if (ui->checkBox_broadcast->isChecked())
	{
		ui->spinBox_dst->setEnabled(false);
		dst = 0xFF;
	}
	else
	{
		ui->spinBox_dst->setEnabled(true);
		dst = ui->spinBox_dst->value();
	}

	serial_thread->queueCommand(NetworkNode::CMD_SET_PACKET_DST, (char*)&dst, 1);
}

//Habilita/deshabilita paquetes personalizados
void MainWindow::updatePacketContent()
{
	unsigned char custom_packet = ui->checkBox_content->isChecked();

	ui->pushButton_content->setEnabled(custom_packet);

	serial_thread->queueCommand(NetworkNode::CMD_SET_CUSTOM_PACKET, (char*)&custom_packet, 1);
}

//Carga datos de paquetes personalizados al dispositivo
void MainWindow::loadPacketData()
{
	QString filename = QFileDialog::getOpenFileName(this,
	                                                tr("Seleccionar archivo fuente"));

	QFile in_file(filename);

	if (in_file.open(QIODevice::ReadOnly))
	{
		uchar payload[1516];

		int filesize = in_file.size();
		if (filesize > 1514)
		{
			filesize = 1524;
		}

		payload[0] = filesize >> 8 & 0xFF;
		payload[1] = filesize & 0xFF;

		in_file.read((char*)payload + 2, filesize);

		serial_thread->queueCommand(NetworkNode::CMD_LOAD_CUSTOM_DATA, (char*)payload, filesize + 2);
	}
}

//Control del registro de mediciones
void MainWindow::controlLogger()
{
	log_status = !log_status;

	ui->label_dataCount_right->setEnabled(log_status);
	ui->pushButton_log->setText(log_status ? "Terminar" : "Comenzar");

	if (!log_status)
	{
		if (log_data.length() > 0)
		{
			QString log_filename = QFileDialog::getSaveFileName(this,
			                                                    tr("Guardar Mediciones"),
			                                                    "log.csv",
			                                                    tr("CSV (*.csv)"));

			if (log_filename != "")
			{
				QFile log_file(log_filename);

				if (log_file.open(QIODevice::WriteOnly | QIODevice::Text))
				{
					QTextStream out(&log_file);

					out << "Marca de tiempo,Nodo,Retardo,Número de Secuencia\n";

					for (auto& data : log_data)
					{
						out << data.timestamp << ','
						    << data.node << ','
						    << data.value << ','
						    << data.sequence << '\n';
					}

					log_file.close();
				}
			}
		}
	}
	else
	{
		log_time.restart();
		log_data.clear();
	}
}

//Activa y desactiva graficos
void MainWindow::selectChart(int index)
{
	latency_bar_chart->setActive(index == 0);
	latency_line_chart->setActive(index == 1);
}

//Manejo de nuevas mediciones
void MainWindow::measurementReceived(uint value, uint sequence, uint node)
{
	//QString status = "[Última medición] " +
	//                 QString::number(device_precision * value) + " ns " +
	//                 "desde " + QString::number(node);

	//ui->statusBar->showMessage(status);
	latency_bar_chart->updateLatency(value * device_precision, sequence, node);
	latency_line_chart->updateLatency(value * device_precision, sequence, node);

	if (log_status)
	{
		Measurement new_measurement;
		new_measurement.timestamp = log_time.elapsed() / 1000.0;
		new_measurement.value = value * device_precision;
		new_measurement.node = node;
		new_measurement.sequence = sequence;

		log_data.push_back(new_measurement);

		ui->label_dataCount_right->setText(QString::number(log_data.length()));
	}
}
