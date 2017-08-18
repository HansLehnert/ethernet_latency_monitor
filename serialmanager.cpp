#include "serialthread.h"

#include <QDebug>

#include "networknode.h"

SerialThread::SerialThread()
{
	serial_port = nullptr;

	refresh_timer.setInterval(2000);
	refresh_timer.setSingleShot(false);
	refresh_timer.start();

	connect(&refresh_timer, SIGNAL(timeout()), this, SLOT(scheduleCheckDevice()));
}

void SerialThread::run()
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
			sendCommand(command_queue[0].command,
			            (char*)&(command_queue[0].data[0]),
			            command_queue[0].data.length());
			command_queue.pop_front();
		}
		command_queue_mutex.unlock();

		//Actualizar registros de ser necesario
		if (check_device_flag)
		{
			checkDevice();
			check_device_flag = false;
		}

		readMeasurements(true);

		msleep(100);
	}

	serial_port->close();

	delete serial_port;
	serial_port = nullptr;

	emit disconnected();
}

void SerialThread::openPort(QString& new_port_name, qint32 new_baudrate)
{
	if (serial_port != nullptr)
	{
		should_exit = true;
	}
	else
	{
		port_name = new_port_name;
		baudrate = new_baudrate;

		start();
	}
}

void SerialThread::checkDevice()
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
}

void SerialThread::queueCommand(char command, const char* data, quint32 data_length)
{
	Command new_command;
	new_command.command = command;
	for (uint i = 0; i < data_length; i++)
		new_command.data.append(data[i]);

	command_queue_mutex.lock();
	command_queue.append(new_command);
	command_queue_mutex.unlock();
}

bool SerialThread::sendCommand(char command, char* data, quint32 data_length, char* response, quint32 response_length)
{
	if (serial_port->isOpen())
	{
		serial_port->write(&command, 1);
		if (serial_port->waitForBytesWritten(100))
		{
			if (data != nullptr && data_length != 0)
			{
				serial_port->write(data, data_length);
				if (!serial_port->waitForBytesWritten(100))
				{
					qDebug() << "Command [" << command << "]: Couldn't write data";
					return false;
				}
			}

			if (response != nullptr && response_length != 0)
			{
				int total_bytes_read = 0;
				int bytes_read;

				while (total_bytes_read != response_length)
				{
					if (serial_port->waitForReadyRead(100))
					{
						bytes_read = serial_port->read(response, response_length);
						response += bytes_read;
						total_bytes_read += bytes_read;
					}
					else
					{
						qDebug() << "Command [" << command << "]: Didn't get response (timeout)";
						return false;
					}
				}
			}

			return true;
		}
		else
		{
			qDebug() << "Command [" << command << "]: Couldn't write command";
			return false;
		}
	}
	return false;
}

void SerialThread::scheduleCheckDevice()
{
	check_device_flag = true;
}

void SerialThread::readMeasurements(bool send_signal)
{
	uchar size_serialized[2];
	uint size;

	if (sendCommand(NetworkNode::CMD_MEASUREMENTS, nullptr, 0, (char*)size_serialized, 2))
	{
		size = (size_serialized[0] << 8) + size_serialized[1];

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
