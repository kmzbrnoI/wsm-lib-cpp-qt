#include <QCoreApplication>
#include <QSerialPort>
#include <QtMath>
#include <cmath>
#include <algorithm>

#include "wsm.h"

namespace Wsm {

Wsm::Wsm(unsigned int scale, double wheelDiameter, size_t ticksPerRevolution, QObject *parent)
	: QObject(parent), scale(scale), wheelDiameter(wheelDiameter),
	  ticksPerRevolution(ticksPerRevolution) {
	m_serialPort.setBaudRate(9600);
	m_serialPort.setFlowControl(QSerialPort::FlowControl::HardwareControl);
	m_serialPort.setReadBufferSize(256);

	QObject::connect(&m_speedTimer, SIGNAL(timeout()), this, SLOT(t_speedTimeout()));
	m_speedTimer.setSingleShot(true);

	QObject::connect(&m_serialPort, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
	QObject::connect(&m_serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)),
	                 this, SLOT(handleError(QSerialPort::SerialPortError)));
}

void Wsm::connect(const QString& portname) {
	m_serialPort.setPortName(portname);

	if (!m_serialPort.open(QIODevice::ReadOnly))
		throw EOpenError(m_serialPort.errorString());

	m_serialPort.clear();
}

void Wsm::disconnect() {
	if (m_speedTimer.isActive())
		m_speedTimer.stop();
	m_serialPort.close();
}

bool Wsm::connected() const {
	return m_serialPort.isOpen();
}

void Wsm::handleReadyRead() {
	// check timeout
	if (m_receiveTimeout < QDateTime::currentDateTime() && m_readData.size() > 0) {
		// clear input buffer when data not received for a long time
		m_readData.clear();
	}

	m_readData.append(m_serialPort.readAll());
	m_receiveTimeout = QDateTime::currentDateTime().addMSecs(BUF_IN_TIMEOUT_MS);

	while (m_readData.size() > 0 && m_readData.size() >= (m_readData[0] & 0x0F)+2) {
		unsigned int length = (m_readData[0] & 0x0F)+2; // including header byte & xor
		uint8_t x = 0;
		for (uint i = 0; i < length; i++)
			x ^= m_readData[i] & 0x7F;

		if (x != 0) {
			// XOR error
			m_readData.remove(0, static_cast<int>(length));
			continue;
		}

		QByteArray message;
		message.reserve(static_cast<int>(length));
		message.setRawData(m_readData.data(), length); // will not copy, we must preserve m_readData!
		parseMessage(message);
		m_readData.remove(0, static_cast<int>(length));
	}
}

void Wsm::handleError(QSerialPort::SerialPortError serialPortError) {
	if (serialPortError != QSerialPort::NoError)
        emit onError(m_serialPort.errorString());
}

void Wsm::parseMessage(QByteArray& message) {
	switch (static_cast<MsgRecvType>((message[0] >> 4) & 0x7)) {
		case MsgRecvType::Speed: return handleMsgSpeed(message);
		case MsgRecvType::Voltage: return handleMsgVoltage(message);
	}
}

void Wsm::handleMsgSpeed(QByteArray& message) {
	switch (static_cast<MsgSpeedType>(static_cast<uint8_t>(message.at(1)))) {
		case MsgSpeedType::Interval: return handleMsgSpeedInterval(message);
		case MsgSpeedType::Distance: return handleMsgSpeedDistance(message);
	}
}

void Wsm::handleMsgSpeedInterval(QByteArray& message) {
	if (!m_speedOk) {
		m_speedOk = true;
        emit speedReceiveRestore();
	}
	m_speedTimer.start(SPEED_RECEIVE_TIMEOUT);

	uint16_t interval = \
			((static_cast<uint8_t>(message[2]) & 0x03) << 14) | \
			((static_cast<uint8_t>(message[3]) & 0x7F) << 7) |  \
			(static_cast<uint8_t>(message[4]) & 0x7F);

	double speed;
	if (interval == 0xFFFF) {
		speed = 0;
	} else {
		speed = (static_cast<double>(M_PI) * wheelDiameter * F_CPU * 3.6 * scale / 1000) /
				(ticksPerRevolution * PSK * interval);
	}
    emit speedRead(speed, interval);
	if (m_lt_measuring)
		recordLt(speed);
}

void Wsm::handleMsgSpeedDistance(QByteArray& message) {
	m_dist = \
			((static_cast<uint8_t>(message[2]) & 0x0F) << 28) | \
			((static_cast<uint8_t>(message[3]) & 0x7F) << 21) | \
			((static_cast<uint8_t>(message[4]) & 0x7F) << 14) | \
			((static_cast<uint8_t>(message[5]) & 0x7F) << 7) |  \
			(static_cast<uint8_t>(message[6]) & 0x7F);

	uint32_t distDelta = m_dist - m_distStart;
    emit distanceRead(calcDist(distDelta), distDelta);
}

void Wsm::handleMsgVoltage(QByteArray& message) {
	uint16_t measured = (static_cast<uint8_t>(message[1] & 0x07) << 7) |
	                    (static_cast<uint8_t>(message[2]) & 0x7F);
	double voltage = (measured * 4.587 / 1024);
    emit batteryRead(voltage, measured);

	bool critical = (message[1] >> 6) & 0x1;
	if (critical)
        emit batteryCritical();
}

void Wsm::distanceReset() {
	m_distStart = m_dist;
}

void Wsm::t_speedTimeout() {
	m_speedOk = false;
	m_lt_measuring = false;
    emit speedReceiveTimeout();
}

bool Wsm::isSpeedOk() const {
	return m_speedOk;
}

void Wsm::startLongTermMeasure(unsigned count) {
	if (m_lt_measuring)
		throw ELtAlreadyMeasuring("Long-term speed measurement is alterady running!");
	if (!m_speedOk)
		throw ENoSpeedData("Cannot init measurement, speed not received!");

	m_lt_count_max = count;
	m_lt_count = 0;
	m_lt_sum = 0;
	m_lt_measuring = true;
}

void Wsm::recordLt(double speed) {
	m_lt_count++;
	m_lt_sum += speed;

	if (m_lt_count == 1)
		m_lt_min = m_lt_max = speed;

	m_lt_max = std::max(speed, m_lt_max);
	m_lt_min = std::min(speed, m_lt_min);

	if (m_lt_count == m_lt_count_max) {
		m_lt_measuring = false;
        emit longTermMeasureDone(m_lt_sum / m_lt_count, std::abs(m_lt_min-m_lt_max));
	}
}

uint32_t Wsm::distRaw() const {
	return m_dist - m_distStart;
}

double Wsm::calcDist(uint32_t rawDelta) const {
	return (rawDelta * static_cast<double>(M_PI) * wheelDiameter) / (1000 * ticksPerRevolution);
}

}//namespace Wsm
