#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>

#include "networknode.h"
#include "serialize.h"
#include "utils.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
	ui->setupUi(this);

	//Hilo de ejecución para comunicacion serial
	serial_thread = new QThread();
	serial_manager.moveToThread(serial_thread);
	connect(serial_thread, &QThread::started,
	        &serial_manager, &SerialManager::run);
	connect(&serial_manager, &SerialManager::start,
	        [this](){ serial_thread->start(); });
	connect(&serial_manager, &SerialManager::disconnected,
	        serial_thread, &QThread::quit);
	connect(qApp, &QCoreApplication::aboutToQuit,
	        &serial_manager, &SerialManager::closePort);

	//Configuración de timer para la actualización de datos del dispositivo
	device_info_timer.setInterval(2000);
	connect(&serial_manager, &SerialManager::connected,
	        &device_info_timer, QOverload<>::of(&QTimer::start));
	connect(&serial_manager, &SerialManager::disconnected,
	        &device_info_timer, &QTimer::stop);
	connect(&device_info_timer, &QTimer::timeout,
	        this, &MainWindow::requestDeviceInfo);

	bandwidth_timer.setInterval(100);
	connect(&serial_manager, &SerialManager::connected,
	        &bandwidth_timer, QOverload<>::of(&QTimer::start));
	connect(&serial_manager, &SerialManager::disconnected,
	        &bandwidth_timer, &QTimer::stop);
	connect(&bandwidth_timer, &QTimer::timeout, [=] () {
		serial_manager.queueCommand(NetworkNode::CMD_GET_BANDWIDTH_IN, {}, 4);
		serial_manager.queueCommand(NetworkNode::CMD_GET_BANDWIDTH_OUT, {}, 4);
	});

	//Graficos
	latency_bar_chart = new LatencyBarChart();
	latency_line_chart = new LatencyLineChart();

	ui->verticalLayout_chart->addWidget(latency_line_chart);
	ui->verticalLayout_chart->addWidget(latency_bar_chart);

	ui->comboBox_chartType->addItem("Barras");
	ui->comboBox_chartType->addItem("Líneas");

	selectChart(ui->comboBox_chartType->currentIndex());

	//Botones
	connect(ui->pushButton_scan, &QPushButton::clicked,
	        this, &MainWindow::scanPorts);
	connect(ui->pushButton_connect, &QPushButton::clicked,
	        this, &MainWindow::connectPort);
	connect(ui->pushButton_log, &QPushButton::clicked,
	        this, &MainWindow::controlLogger);

	//Conexión y desconexión de interfaz serial
	connect(&serial_manager, &SerialManager::connected,
	        this, &MainWindow::portStatusChanged);
	connect(&serial_manager, &SerialManager::disconnected,
	        this, &MainWindow::portStatusChanged);

	//connect(this, SIGNAL(requestDeviceInfo()), serial_manager, SLOT(scheduleCheckDevice()));
	connect(&serial_manager, &SerialManager::commandResponse,
	        this, &MainWindow::deviceResponseReceived);

	//Configuración del dispositivo
	connect(ui->spinBox_packetInterval, QOverload<int>::of(&QSpinBox::valueChanged),
	        [this] (int value) { serial_manager.queueCommand(NetworkNode::CMD_SET_PACKET_INTERVAL, serialize<ushort>(value), 0); });
	connect(ui->spinBox_packetSize, QOverload<int>::of(&QSpinBox::valueChanged),
	        [this] (int value) { serial_manager.queueCommand(NetworkNode::CMD_SET_PACKET_SIZE, serialize<ushort>(value), 0); });
	connect(ui->spinBox_dst, QOverload<int>::of(&QSpinBox::valueChanged),
	        this, &MainWindow::updatePacketDst);
	connect(ui->checkBox_broadcast, &QCheckBox::stateChanged,
	        this, &MainWindow::updatePacketDst);
	connect(ui->checkBox_content, &QCheckBox::stateChanged,
	        this, &MainWindow::updatePacketContent);
	connect(ui->pushButton_content, &QPushButton::clicked,
	        this, &MainWindow::loadPacketData);

	connect(ui->comboBox_chartType, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &MainWindow::selectChart);

	//Notificación de mediciones
	connect(&serial_manager, &SerialManager::newMeasurement,
	        this, &MainWindow::measurementReceived);
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
	serial_manager.openPort(port_name, baudrate);
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
		requestDeviceInfo();
	}
	else
	{
		ui->pushButton_connect->setText("Conectar");
	}
}

//Actulaizar interfaz al recibir lecturas de los registros del dispositivo
void MainWindow::deviceResponseReceived(NetworkNode::Command command, QVector<uchar> data)
{
	QString text;
	qint64 value;

	switch (command) {
	case NetworkNode::CMD_GET_ID:
		text = QString::number(data[0]);
		ui->label_id_right->setText(text);
		break;

	case NetworkNode::CMD_GET_VERSION:
		text = QString::number(data[0]) + "." +
		       QString::number(data[1]);
		ui->label_version_right->setText(text);
		break;

	case NetworkNode::CMD_GET_PRECISION:
		value = deserialize<uint>(data);
		device_precision = value != 0 ? 1000000000 / value : -1;
		text = metricPrefix(1 / double(value)) + "s";
		ui->label_precision_right->setText(text);
		break;

	case NetworkNode::CMD_GET_MAC_ADDRESS:
		text = QString("%1").arg(data[0], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[1], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[2], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[3], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[4], 2, 16, QChar('0')) + ":" +
		       QString("%1").arg(data[5], 2, 16, QChar('0'));
		ui->label_mac_right->setText(text);
		break;

	case NetworkNode::CMD_GET_PACKET_SIZE:
		value = deserialize<ushort>(data);
		ui->spinBox_packetSize->setValue(value);
		break;

	case NetworkNode::CMD_GET_PACKET_INTERVAL:
		value = deserialize<ushort>(data);
		ui->spinBox_packetInterval->setValue(value);
		break;

	case NetworkNode::CMD_GET_PACKET_DST:
		if (data[0] == 0xFF) {
			ui->checkBox_broadcast->setChecked(true);
		}
		else {
			ui->checkBox_broadcast->setChecked(false);
		}
		break;

	case NetworkNode::CMD_GET_CUSTOM_PACKET:
		ui->checkBox_content->setChecked(data[0] == 0x01);
		break;

	case NetworkNode::CMD_GET_BANDWIDTH_IN:
		ui->label_bandwidth_in_bottom->setText(metricPrefix(deserialize<uint>(data) / 0.1) + "B/s");
		break;

	case NetworkNode::CMD_GET_BANDWIDTH_OUT:
		ui->label_bandwidth_out_bottom->setText(metricPrefix(deserialize<uint>(data) / 0.1) + "B/s");
		break;
	}
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

	serial_manager.queueCommand(NetworkNode::CMD_SET_PACKET_DST, {dst}, 0);
}


//Habilita/deshabilita paquetes personalizados
void MainWindow::updatePacketContent()
{
	unsigned char custom_packet = ui->checkBox_content->isChecked();

	ui->pushButton_content->setEnabled(custom_packet);

	serial_manager.queueCommand(NetworkNode::CMD_SET_CUSTOM_PACKET, {custom_packet}, 0);
}


//Realiza la lectura de los registros del dispositivo
void MainWindow::requestDeviceInfo() {
	serial_manager.queueCommand(NetworkNode::CMD_GET_ID, {}, 1);
	serial_manager.queueCommand(NetworkNode::CMD_GET_VERSION, {}, 2);
	serial_manager.queueCommand(NetworkNode::CMD_GET_PRECISION, {}, 4);
	serial_manager.queueCommand(NetworkNode::CMD_GET_MAC_ADDRESS, {}, 6);
	serial_manager.queueCommand(NetworkNode::CMD_GET_PACKET_SIZE, {}, 2);
	serial_manager.queueCommand(NetworkNode::CMD_GET_PACKET_INTERVAL, {}, 2);
	serial_manager.queueCommand(NetworkNode::CMD_GET_PACKET_DST, {}, 1);
	serial_manager.queueCommand(NetworkNode::CMD_GET_CUSTOM_PACKET, {}, 1);
}


//Carga datos de paquetes personalizados al dispositivo
void MainWindow::loadPacketData()
{
	QString filename = QFileDialog::getOpenFileName(this, tr("Seleccionar archivo fuente"));

	QFile in_file(filename);

	if (in_file.open(QIODevice::ReadOnly))
	{
		SerialManager::Command command;
		command.command = NetworkNode::Command::CMD_LOAD_CUSTOM_DATA;
		command.response_length = 0;

		int filesize = in_file.size();
		if (filesize > 1514)
		{
			filesize = 1514;
		}

		command.data.resize(filesize + 2);

		command.data[0] = filesize >> 8 & 0xFF;
		command.data[1] = filesize & 0xFF;

		in_file.read(reinterpret_cast<char*>(&command.data[2]), filesize);

		serial_manager.queueCommand(command);
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
