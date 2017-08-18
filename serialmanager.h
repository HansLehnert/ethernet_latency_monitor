#ifndef SERIALTHREAD_H
#define SERIALTHREAD_H

#include <QThread>
#include <QSerialPort>
#include <QTimer>
#include <QMutex>
#include <QVector>

class SerialThread : public QThread
{
	Q_OBJECT

public:
	SerialThread();
	void run() Q_DECL_OVERRIDE;

private:
	QTimer refresh_timer;

	//Puerto Serial
	QSerialPort* serial_port;
	QString port_name;
	qint32 baudrate;

	//Cola de Comandos
	struct Command {
		uchar command;
		QVector<uchar> data;
	};
	QList<Command> command_queue;
	QMutex command_queue_mutex;

	//Banderas
	bool should_exit;
	bool check_device_flag;

	void checkDevice();
	//Revisa todos los registros del dispositivo

	bool sendCommand(char, char*, quint32, char* = nullptr, quint32 = 0);
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

	void deviceRegisterUpdated(quint32, QVector<uchar>);
	//Emitida tras la lectura de un registro

	void newMeasurement(uint, uint, uint);

public slots:
	void openPort(QString&, qint32);
	//Slot para inicio de la conexión

	void scheduleCheckDevice();
	//Revisar los valores de los registros

	void queueCommand(char, const char*, quint32);
	//Poner un comando en la cola

};

#endif // SERIALTHREAD_H
