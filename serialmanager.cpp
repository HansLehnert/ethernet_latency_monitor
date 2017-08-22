#include "serialmanager.h"

#include <QDebug>
#include <QThread>

#include "networknode.h"

SerialManager::SerialManager()
{
	serial_port = nullptr;

	/*refresh_timer.setInterval(2000);
	refresh_timer.setSingleShot(false);
	refresh_timer.start();

	connect(&refresh_timer, SIGNAL(timeout()), this, SLOT(readRegister()));*/
}

void SerialManager::run()
{
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
		command_queue_mutex.lock();
		while(command_queue.length() > 0)
		{
			QVector<uchar> response;
			sendCommand(command_queue[0], &response);

			if (response.length() > 0)
				emit commandResponse(command_queue[0].command, response);
			command_queue.pop_front();
		}
		command_queue_mutex.unlock();

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


/*void SerialManager::checkDevice()
{
	if (serial_port->isOpen())
	{
		QVector<uchar> response;
		response.fill(0, 8);

		if (sendCommand(NetworkNode::CMD_GET_ID, nullptr, 0, (char*) &response[0], 1))
			emit deviceRegisterUpdated(NetworkNode::REGISTER_ID, response);

		if (sendCommand(NetworkNode::CMD_GET_VERSION, nullptr, 0, (char*) &response[0], 2))
			emit deviceRegisterUpdated(NetworkNode::REGISTER_VERSION, response);

		if (sendCommand(NetworkNode::CMD_GET_PRECISION, nullptr, 0, (char*) &response[0], 4))
			emit deviceRegisterUpdated(NetworkNode::REGISTER_PRECISION, response);

		if (sendCommand(NetworkNode::CMD_GET_MAC_ADDRESS, nullptr, 0, (char*) &response[0], 6))
			emit deviceRegisterUpdated(NetworkNode::REGISTER_MAC_ADDRESS, response);

		if (sendCommand(NetworkNode::CMD_GET_PACKET_SIZE, nullptr, 0, (char*) &response[0], 2))
			emit deviceRegisterUpdated(NetworkNode::REGISTER_PACKET_SIZE, response);

		if (sendCommand(NetworkNode::CMD_GET_PACKET_INTERVAL, nullptr, 0, (char*) &response[0], 2))
			emit deviceRegisterUpdated(NetworkNode::REGISTER_PACKET_INTERVAL, response);

		if (sendCommand(NetworkNode::CMD_GET_PACKET_DST, nullptr, 0, (char*) &response[0], 1))
			emit deviceRegisterUpdated(NetworkNode::REGISTER_PACKET_DST, response);

		if (sendCommand(NetworkNode::CMD_GET_CUSTOM_PACKET, nullptr, 0, (char*) &response[0], 1))
			emit deviceRegisterUpdated(NetworkNode::REGISTER_CUSTOM_PACKET, response);
	}
}*/


void SerialManager::queueCommand(Command command)
{
	command_queue_mutex.lock();
	command_queue.append(command);
	command_queue_mutex.unlock();
}


void SerialManager::queueCommand(NetworkNode::Command command, QVector<uchar> data, uint response_length) {
	queueCommand({command, data, response_length});
}


bool SerialManager::sendCommand(Command command, QVector<uchar>* response)
{
	if (!serial_port->isOpen())
		return false;

	//Enviar byte correspondiente al comando
	uchar command_byte = (uchar)command.command;
	serial_port->write(reinterpret_cast<char*>(&command_byte), 1);
	if (!(serial_port->waitForBytesWritten(100)))
	{
		qDebug() << "Command [" << command.command << "]: Couldn't write command";
		return false;
	}

	//Enviar los datos asociados al comando
	if (command.data.length() != 0)
	{
		serial_port->write(reinterpret_cast<char*>(&command.data[0]), command.data.length());
		if (!serial_port->waitForBytesWritten(100))
		{
			qDebug() << "Command [" << command.command << "]: Couldn't write data";
			return false;
		}
	}

	//Recibir respuesta del comando
	if (response != nullptr && command.response_length != 0)
	{
		response->resize(command.response_length);

		quint32 bytes_read = 0;

		while (bytes_read < command.response_length)
		{
			if (serial_port->waitForReadyRead(100))
			{
				bytes_read += serial_port->read(reinterpret_cast<char*>(&(*response)[0]) + bytes_read, command.response_length - bytes_read);
			}
			else
			{
				qDebug() << "Command [" << command.command << "]: Didn't get response (timeout)";
				return false;
			}
		}
	}

	return true;
}


void SerialManager::readMeasurements(bool send_signal)
{
	Command command;
	command.command = NetworkNode::Command::CMD_MEASUREMENTS;
	command.response_length = 2;
	QVector<uchar> response;

	if (sendCommand(command, &response))
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
