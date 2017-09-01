#include "serialmanager.h"

#include <QDebug>
#include <QThread>

#include "networknode.h"

SerialManager::SerialManager() {
	serial_port = nullptr;

	/*refresh_timer.setInterval(2000);
	refresh_timer.setSingleShot(false);
	refresh_timer.start();

	connect(&refresh_timer, SIGNAL(timeout()), this, SLOT(readRegister()));*/
}


void SerialManager::run() {
	serial_port = new QSerialPort();

	serial_port->setPortName(port_name);
	serial_port->setBaudRate(baudrate);
	serial_port->setParity(QSerialPort::EvenParity);
	serial_port->setStopBits(QSerialPort::OneStop);

	if (serial_port->open(QIODevice::ReadWrite))
	{
		emit connected();
	}
	else
	{
		serial_port->close();
		delete serial_port;
		serial_port = nullptr;
		return;
	}

	//Descartar mediciones almacenadas inicialmente
	readMeasurements(false);

	should_exit = false;
	while (!should_exit)
	{
		serial_port->clear();

		//Enviar comandos en cola
		query_queue_mutex.lock();
		while(query_queue.length() > 0)
		{
			QVector<uchar> response;
			sendQuery(query_queue[0], &response);

			if (response.length() > 0)
				emit queryResponse(query_queue[0].command, response);
			query_queue.pop_front();
		}
		query_queue_mutex.unlock();

		//Actualizar registros de ser necesario
		/*if (check_device_flag)
		{
			checkDevice();
			check_device_flag = false;
		}*/

		readMeasurements(true);

		QThread::msleep(100);
	}

	serial_port->close();

	delete serial_port;
	serial_port = nullptr;

	emit disconnected();
}


void SerialManager::openPort(QString& new_port_name, qint32 new_baudrate)
{
	if (serial_port != nullptr)
	{
		should_exit = true;
	}
	else
	{
		port_name = new_port_name;
		baudrate = new_baudrate;
		should_exit = false;

		start();
	}
}


void SerialManager::closePort()
{
	should_exit = true;
}


void SerialManager::queueQuery(NetworkNode::Query query) {
	query_queue_mutex.lock();
	query_queue.append(query);
	query_queue_mutex.unlock();
}


void SerialManager::queueQuery(NetworkNode::Command command, QVector<uchar> data, uint response_length) {
	queueQuery({command, data, response_length});
}


bool SerialManager::sendQuery(NetworkNode::Query query, QVector<uchar>* response) {
	if (!serial_port->isOpen())
		return false;

	//Enviar byte correspondiente al comando
	uchar command_byte = (uchar)query.command;
	serial_port->write(reinterpret_cast<char*>(&command_byte), 1);
	if (!(serial_port->waitForBytesWritten(100)))
	{
		qDebug() << "Command [" << query.command << "]: Couldn't write command";
		return false;
	}

	//Enviar los datos asociados al comando
	if (query.data.length() != 0)
	{
		serial_port->write(reinterpret_cast<char*>(&query.data[0]), query.data.length());
		if (!serial_port->waitForBytesWritten(100))
		{
			qDebug() << "Command [" << query.command << "]: Couldn't write data";
			return false;
		}
	}

	//Recibir respuesta del comando
	if (response != nullptr && query.response_length != 0)
	{
		response->resize(query.response_length);

		quint32 bytes_read = 0;

		while (bytes_read < query.response_length)
		{
			if (serial_port->waitForReadyRead(100))
			{
				bytes_read += serial_port->read(reinterpret_cast<char*>(&(*response)[0]) + bytes_read, query.response_length - bytes_read);
			}
			else
			{
				qDebug() << "Command [" << query.command << "]: Didn't get response (timeout)";
				return false;
			}
		}
	}

	return true;
}


void SerialManager::readMeasurements(bool send_signal) {
	NetworkNode::Query query;
	query.command = NetworkNode::Command::CMD_MEASUREMENTS;
	query.response_length = 2;
	QVector<uchar> response;

	if (sendQuery(query, &response))
	{
		uint size = (response[0] << 8) + response[1];

		uint bytes_read = 0;
		while (bytes_read < size)
		{
			if (send_signal)
			{
				uchar command_serialized[9];
				uint command_bytes_read = 0;

				while (command_bytes_read < 9)
				{
					/*if (!serial_port->waitForReadyRead(10))
					{
						return;
					}*/
					command_bytes_read += serial_port->read((char*)(command_serialized + command_bytes_read),
					                                        9 - command_bytes_read);
				}

				uint latency = (command_serialized[0] << 24) +
				               (command_serialized[1] << 16) +
				               (command_serialized[2] << 8) +
				               command_serialized[3];
				uint sequence = (command_serialized[4] << 24) +
				                (command_serialized[5] << 16) +
				                (command_serialized[6] << 8) +
				                command_serialized[7];
				uint node = command_serialized[8];

				emit newMeasurement(latency, sequence, node);

				bytes_read += 9;
			}
			else
			{
				serial_port->clear();
				break;
			}
		}
	}
}
