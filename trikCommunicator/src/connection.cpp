/* Copyright 2014 CyberTech Labs Ltd.
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

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

#include <trikKernel/fileUtils.h>
#include <trikScriptRunner/trikScriptRunner.h>

#include "src/connection.h"

using namespace trikCommunicator;

Connection::Connection()
	: mTrikScriptRunner(nullptr)
{
}

void Connection::sendMessage(QString const &message)
{
	mSocket->write(message.toLocal8Bit());
}

void Connection::init(int socketDescriptor, trikScriptRunner::TrikScriptRunner *trikScriptRunner)
{
	mTrikScriptRunner = trikScriptRunner;
	mSocket.reset(new QTcpSocket());

	if (!mSocket->setSocketDescriptor(socketDescriptor)) {
		qDebug() << "Failed to set socket descriptor" << socketDescriptor;
		return;
	}

	connect(mSocket.data(), SIGNAL(readyRead()), this, SLOT(onReadyRead()));
	connect(mSocket.data(), SIGNAL(disconnected()), this, SLOT(disconnected()));
}

void Connection::onReadyRead()
{
	if (!mSocket || !mSocket->isValid()) {
		return;
	}

	QByteArray const &data = mSocket->readAll();
	QString command = QString::fromUtf8(data.constData());

	if (!command.startsWith("keepalive")) {
		// Discard "keepalive" output.
		qDebug() << "Command: " << command;
	}

	if (command.startsWith("file")) {
		command.remove(0, QString("file:").length());
		int const separatorPosition = command.indexOf(':');
		if (separatorPosition == -1) {

			qDebug() << "Malformed 'file' command";

			return;
		}

		QString const fileName = command.left(separatorPosition);
		QString const fileContents = command.mid(separatorPosition + 1);
		trikKernel::FileUtils::writeToFile(fileName, fileContents);
	} else if (command.startsWith("run")) {
		command.remove(0, QString("run:").length());
		QString const fileContents = trikKernel::FileUtils::readFromFile(command);
		QMetaObject::invokeMethod(mTrikScriptRunner, "run", Q_ARG(QString, fileContents));
		emit startedScript(command);
	} else if (command == "stop") {
		QMetaObject::invokeMethod(mTrikScriptRunner, "abort");
	} else if (command.startsWith("direct")) {
		command.remove(0, QString("direct:").length());
		QMetaObject::invokeMethod(mTrikScriptRunner, "run", Q_ARG(QString, command));
		emit startedDirectScript();
	}
}

void Connection::disconnected()
{
	qDebug() << "Disconnected.";
	thread()->quit();
}
