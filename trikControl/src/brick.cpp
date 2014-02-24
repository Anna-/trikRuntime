/* Copyright 2013 Yurii Litvinov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#include "brick.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QFileInfo>

#include "configurer.h"
#include "i2cCommunicator.h"

using namespace trikControl;

Brick::Brick(QThread &guiThread, QString const &configFilePath)
	: mConfigurer(new Configurer(configFilePath))
	, mI2cCommunicator(NULL)
	, mDisplay(guiThread)
	, mInEventDrivenMode(false)
{
	if (::system(mConfigurer->initScript().toStdString().c_str()) != 0) {
		qDebug() << "Init script failed";
	}

	mI2cCommunicator = new I2cCommunicator(mConfigurer->i2cPath(), mConfigurer->i2cDeviceId());

	foreach (QString const &port, mConfigurer->servoMotorPorts()) {
		QString const motorType = mConfigurer->servoMotorDefaultType(port);

		ServoMotor *servoMotor = new ServoMotor(
				mConfigurer->motorTypeMin(motorType)
				, mConfigurer->motorTypeMax(motorType)
				, mConfigurer->motorTypeZero(motorType)
				, mConfigurer->motorTypeStop(motorType)
				, mConfigurer->servoMotorDeviceFile(port)
				, mConfigurer->servoMotorPeriodFile(port)
				, mConfigurer->servoMotorPeriod(port)
				, mConfigurer->servoMotorInvert(port)
				);

		mServoMotors.insert(port, servoMotor);
	}

	foreach (QString const &port, mConfigurer->pwmCapturePorts()) {
		PwmCapture *pwmCapture = new PwmCapture(
				mConfigurer->pwmCaptureFrequencyFile(port)
				, mConfigurer->pwmCaptureDutyFile(port)
				);

		mPwmCaptures.insert(port, pwmCapture);
	}

	foreach (QString const &port, mConfigurer->powerMotorPorts()) {
		PowerMotor *powerMotor = new PowerMotor(
				*mI2cCommunicator
				, mConfigurer->powerMotorI2cCommandNumber(port)
				, mConfigurer->powerMotorInvert(port)
				);

		mPowerMotors.insert(port, powerMotor);
	}

	foreach (QString const &port, mConfigurer->analogSensorPorts()) {
		AnalogSensor *analogSensor = new AnalogSensor(
			*mI2cCommunicator
			, mConfigurer->analogSensorI2cCommandNumber(port)
			);

		mAnalogSensors.insert(port, analogSensor);
	}

	foreach (QString const &port, mConfigurer->sensorPorts()) {
		QString const sensorType = mConfigurer->sensorDefaultType(port);

		Sensor *sensor = new Sensor(
				mConfigurer->sensorTypeMin(sensorType)
				, mConfigurer->sensorTypeMax(sensorType)
				, mConfigurer->sensorDeviceFile(port)
				);

		mSensors.insert(port, sensor);
	}

	foreach (QString const &port, mConfigurer->encoderPorts()) {
		Encoder *encoder = new Encoder(*mI2cCommunicator, mConfigurer->encoderI2cCommandNumber(port));
		mEncoders.insert(port, encoder);
	}

	mBattery = new Battery(*mI2cCommunicator);

	mAccelerometer = new Sensor3d(mConfigurer->accelerometerMin()
			, mConfigurer->accelerometerMax()
			, mConfigurer->accelerometerDeviceFile()
			);

	mGyroscope = new Sensor3d(mConfigurer->gyroscopeMin()
			, mConfigurer->gyroscopeMax()
			, mConfigurer->gyroscopeDeviceFile()
			);

	mKeys = new Keys(mConfigurer->keysDeviceFile());

	mLed = new Led(mConfigurer->ledRedDeviceFile()
			, mConfigurer->ledGreenDeviceFile()
			, mConfigurer->ledOn()
			, mConfigurer->ledOff()
			);

	mGamepad = new Gamepad(mConfigurer->gamepadPort());

	connect(&mWaitingTimer, SIGNAL(timeout()), this, SLOT(stopWaiting()));
	mWaitingTimer.setSingleShot(true);
}

Brick::~Brick()
{
	delete mConfigurer;
	qDeleteAll(mServoMotors);
	qDeleteAll(mPwmCaptures);
	qDeleteAll(mPowerMotors);
	qDeleteAll(mEncoders);
	qDeleteAll(mSensors);
	delete mAccelerometer;
	delete mGyroscope;
	delete mBattery;
	delete mI2cCommunicator;
	delete mLed;
	delete mKeys;
	delete mGamepad;
}

void Brick::playSound(QString const &soundFileName)
{
	qDebug() << soundFileName;

	QFileInfo const fileInfo(soundFileName);

	QString command;

	if (fileInfo.suffix() == "wav") {
		command = mConfigurer->playWavFileCommand().arg(soundFileName);
	} else if (fileInfo.suffix() == "mp3") {
		command = mConfigurer->playMp3FileCommand().arg(soundFileName);
	}

	if (command.isEmpty() || ::system(command.toStdString().c_str()) != 0) {
		qDebug() << "Play sound failed";
	}
}

void Brick::stop()
{
	foreach (ServoMotor * const servoMotor, mServoMotors.values()) {
		servoMotor->powerOff();
	}

	foreach (PowerMotor * const powerMotor, mPowerMotors.values()) {
		powerMotor->powerOff();
	}

	mLed->red();
	mDisplay.hide();
	stopWaiting();
}

Motor *Brick::motor(QString const &port)
{
	if (mPowerMotors.contains(port)) {
		return mPowerMotors[port];
	} else if (mServoMotors.contains(port)) {
		return mServoMotors[port];
	} else {
		return NULL;
	}
}

PwmCapture *Brick::pwmCapture(QString const &port)
{
	return mPwmCaptures.value(port, NULL);
}

AnalogSensor *Brick::analogSensor(QString const &port)
{
	return mAnalogSensors.value(port, NULL);
}

Sensor *Brick::sensor(QString const &port)
{
	return mSensors.value(port, NULL);
}

QStringList Brick::motorPorts(Motor::Type type) const
{
	switch (type) {
		case Motor::powerMotor: {
			return mPowerMotors.keys();
		}
		case Motor::servoMotor: {
			return mServoMotors.keys();
		}
	}

	return QStringList();
}

QStringList Brick::pwmCapturePorts() const
{
	return mPwmCaptures.keys();
}

QStringList Brick::analogSensorPorts() const
{
	return mAnalogSensors.keys();
}

QStringList Brick::sensorPorts() const
{
	return mSensors.keys();
}

Encoder *Brick::encoder(QString const &port)
{
	return mEncoders.value(port, NULL);
}

Battery *Brick::battery()
{
	return mBattery;
}

Sensor3d *Brick::accelerometer()
{
	return mAccelerometer;
}

Sensor3d *Brick::gyroscope()
{
	return mGyroscope;
}

Keys* Brick::keys()
{
	return mKeys;
}

Gamepad* Brick::gamepad()
{
	return mGamepad;
}

void Brick::wait(int milliseconds)
{
	qDebug() << "wait" << milliseconds;

	mWaitingTimer.start(milliseconds);
	mWaitingEventLoop.exec();
}

qint64 Brick::time() const
{
	return QDateTime::currentMSecsSinceEpoch();
}

Display *Brick::display()
{
	return &mDisplay;
}

Led *Brick::led()
{
	return mLed;
}

void Brick::run()
{
	mInEventDrivenMode = true;
}

bool Brick::isInEventDrivenMode() const
{
	return mInEventDrivenMode;
}

void Brick::quit()
{
	emit quitSignal();
}

void Brick::system(QString const &command)
{
	QStringList args;
	args << "-c" << command;
	qDebug() << "Running:" << "sh" << args;
	QProcess::startDetached("sh", args);
}

void Brick::resetEventDrivenMode()
{
	mInEventDrivenMode = false;
}

void Brick::stopWaiting()
{
	qDebug() << "stopWaiting";

	mWaitingTimer.stop();

	if (mWaitingEventLoop.isRunning()) {
		mWaitingEventLoop.exit();
	}
}
