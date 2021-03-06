#ifndef WSM_H
#define WSM_H

/*
This file implements a Wsm class which represents API to the Wireless
SpeedoMeter.

Just connect to the serial port by calling 'connect' method and wait for
signals. Wsm communicates only in direction Wsm -> PC, so almost all the
information is transmitted to the user via Qt signals.

Long-term-measure allows to measure speed in long time and then average measured
speed and calculate abs(min-max), so the user knows diffusion.
*/

#include <QByteArray>
#include <QSerialPort>
#include <QTimer>
#include <QDateTime>

#include "q-str-exception.h"

#define WSM_VERSION_MAJOR 1
#define WSM_VERSION_MINOR 1

namespace Wsm {

constexpr unsigned int BUF_IN_TIMEOUT_MS = 60;
constexpr unsigned int SPEED_RECEIVE_TIMEOUT = 3000; // 3 s

struct EOpenError : public QStrException {
	EOpenError(const QString str) : QStrException(str) {}
};
struct ELtAlreadyMeasuring : public QStrException {
	ELtAlreadyMeasuring(const QString str) : QStrException(str) {}
};
struct ENoSpeedData : public QStrException {
	ENoSpeedData(const QString str) : QStrException(str) {}
};

enum class MsgRecvType {
	Speed = 0x1,
	Voltage = 0x2,
};

enum class MsgSpeedType {
	Interval = 0x81,
	Distance = 0x82,
};

class Wsm : public QObject {
	Q_OBJECT

public:
	static constexpr unsigned _VERSION_MAJOR = WSM_VERSION_MAJOR;
	static constexpr unsigned _VERSION_MINOR = WSM_VERSION_MINOR;

	static constexpr size_t DEFAULT_SCALE = 120;
	static constexpr double DEFAULT_WHEEL_DIAMETER = 8.0;
	static constexpr size_t DEFAULT_TICKS_PER_REVOLUTION = 8;

	unsigned int scale;
	double wheelDiameter; // unit: mm
	size_t ticksPerRevolution;

	explicit Wsm(unsigned int scale=DEFAULT_SCALE,
	             double wheelDiameter=DEFAULT_WHEEL_DIAMETER,
	             size_t ticksPerRevolution=DEFAULT_TICKS_PER_REVOLUTION,
	             QObject *parent = nullptr);
	void distanceReset();
	void startLongTermMeasure(unsigned count);
	bool isSpeedOk() const;
	uint32_t distRaw() const;
	double calcDist(uint32_t rawDelta) const;

	void connect(const QString& portname);
	void disconnect();
	bool connected() const;

private slots:
	void handleReadyRead();
	void handleError(QSerialPort::SerialPortError error);
	void t_speedTimeout();

signals:
	void speedRead(double speed, uint16_t speed_raw);
	void onError(QString error);
	void batteryRead(double voltage, uint16_t voltage_raw);
	void batteryCritical(); // device will automatically disconnect when this event happens
	void distanceRead(double distance, uint32_t distance_raw);
	void longTermMeasureDone(double speed, double diffusion);
	void speedReceiveTimeout();
	void speedReceiveRestore();

private:
	static constexpr unsigned int F_CPU = 3686400; // unit: Hz
	static constexpr unsigned int PSK = 64;

	QSerialPort m_serialPort;
	QByteArray m_readData;
	QDateTime m_receiveTimeout;
	uint32_t m_distStart = 0;
	uint32_t m_dist = 0;
	QTimer m_speedTimer;
	bool m_speedOk = false;
	bool m_lt_measuring = false;
	double m_lt_sum = 0;
	unsigned m_lt_count = 0;
	unsigned m_lt_count_max = 0;
	double m_lt_min = 0, m_lt_max = 0;

	void parseMessage(QByteArray& message);
	void recordLt(double speed);

	void handleMsgSpeed(QByteArray& message);
	void handleMsgVoltage(QByteArray& message);
	void handleMsgSpeedInterval(QByteArray& message);
	void handleMsgSpeedDistance(QByteArray& message);
};

}//namespace Wsm

#endif
