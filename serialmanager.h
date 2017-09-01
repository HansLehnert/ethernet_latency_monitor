#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QSerialPort>
#include <QTimer>
#include <QMutex>
#include <QVector>

#include "networknode.h"

class SerialManager : public QObject
{
	Q_OBJECT

public:
	SerialManager();

private:
	//Puerto Serial
	QSerialPort* serial_port;
	QString port_name;
	qint32 baudrate;

	//Cola de Comandos
	QList<NetworkNode::Query> query_queue;
	QMutex query_queue_mutex;

	//Banderas
	bool should_exit;
	bool check_device_flag;

	//void checkDevice();
	//Revisa todos los registros del dispositivo

	bool sendQuery(NetworkNode::Query, QVector<uchar>* = nullptr);
	//Envía un comando al dispositivo, junto con datos asociados
	//y recibe una respuesta. Los tamaños de los datos enviados y recibidos
	//deben ser previamente conocidos

	void readMeasurements(bool);
	//Lee todas las mediciones almacenadas y emite las señales correspondientes
	//(si el argumento es "true")

signals:
	void connected(bool = true);
	//Emitida al abrisrse el puerto serial

	void disconnected(bool = false);
	//Emitida al cerrarse el puerto serial

	void start();
	//Emitida para indicar que se debe comenzar el hilo de ejecución

	void queryResponse(NetworkNode::Command, QVector<uchar>);
	//Emitida tras la lectura de un registro

	void newMeasurement(uint, uint, uint);

public slots:
	void run();

	void openPort(QString&, qint32);
	//Slot para inicio y fin de la conexión

	void closePort();
	//Para cerrar la conexión

	void queueQuery(NetworkNode::Query);
	void queueQuery(NetworkNode::Command, QVector<uchar>, uint);
	//Agregar un comando a la cola
};

#endif // SERIALTHREAD_H
