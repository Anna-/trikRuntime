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

#include "controller.h"

#include <QtCore/QProcess>
#include <QtCore/QFileInfo>
#include <QtCore/QDebug>

#include <trikKernel/fileUtils.h>

#include "runningWidget.h"

using namespace trikGui;

int const communicatorPort = 8888;
int const telemetryPort = 9000;

Controller::Controller(QString const &configPath, QString const &startDirPath)
	: mBrick(*thread(), configPath, startDirPath)
	, mScriptRunner(mBrick, startDirPath)
	, mCommunicator(mScriptRunner)
	, mTelemetry(mBrick)
	, mStartDirPath(startDirPath)
{
	connect(&mScriptRunner, SIGNAL(completed(QString, int)), this, SLOT(scriptExecutionCompleted(QString, int)));

	connect(&mScriptRunner, SIGNAL(startedScript(QString, int))
			, this, SLOT(scriptExecutionFromFileStarted(QString, int)));
	connect(&mScriptRunner, SIGNAL(startedDirectScript(int))
			, this, SLOT(directScriptExecutionStarted(int)));

	connect(&mBrick, SIGNAL(stopped()), this, SIGNAL(brickStopped()));

	mCommunicator.startServer(communicatorPort);
	mTelemetry.startServer(telemetryPort);
}

Controller::~Controller()
{
}

void Controller::runFile(QString const &filePath)
{
	QFileInfo const fileInfo(filePath);
	if (fileInfo.suffix() == "qts" || fileInfo.suffix() == "js") {
		mScriptRunner.run(trikKernel::FileUtils::readFromFile(fileInfo.canonicalFilePath()), fileInfo.baseName());
	} else if (fileInfo.suffix() == "wav" || fileInfo.suffix() == "mp3") {
		mScriptRunner.run("brick.playSound(\"" + fileInfo.canonicalFilePath() + "\");", fileInfo.baseName());
	} else if (fileInfo.suffix() == "sh") {
		QStringList args;
		args << filePath;
		QProcess::startDetached("sh", args);
	} else if (fileInfo.isExecutable()) {
		QProcess::startDetached(filePath);
	}
}

void Controller::abortExecution()
{
	mScriptRunner.abort();

	// Now script engine will stop (after some time maybe) and send "completed" signal, which will be caught and
	// processed as if a script finished by itself.
}

trikControl::Brick &Controller::brick()
{
	return mBrick;
}

QString Controller::startDirPath() const
{
	return mStartDirPath;
}

QString Controller::scriptsDirPath() const
{
	return mScriptRunner.scriptsDirPath();
}

QString Controller::scriptsDirName() const
{
	return mScriptRunner.scriptsDirName();
}

void Controller::doCloseRunningWidget(MainWidget &widget)
{
	widget.releaseKeyboard();
	emit closeRunningWidget(widget);
}

void Controller::scriptExecutionCompleted(QString const &error, int scriptId)
{
	if (mRunningWidgets.value(scriptId, nullptr) && error.isEmpty()) {
		doCloseRunningWidget(*mRunningWidgets[scriptId]);

		// Here we can be inside handler of mRunningWidget key press event.
		mRunningWidgets[scriptId]->deleteLater();
		mRunningWidgets.remove(scriptId);
	} else if (!error.isEmpty()) {
		if (mRunningWidgets[scriptId]->isVisible()) {
			mRunningWidgets[scriptId]->showError(error);
			mCommunicator.sendMessage("error: " + error);
		} else {
			// It is already closed so all we need is to delete it.
			mRunningWidgets[scriptId]->deleteLater();
			mRunningWidgets.remove(scriptId);
		}
	}
}

void Controller::scriptExecutionFromFileStarted(QString const &fileName, int scriptId)
{
	qDebug() << "Controller::scriptExecutionFromFileStarted : " << fileName << scriptId;
	if (mRunningWidgets.value(scriptId, nullptr)) {
		emit closeRunningWidget(*mRunningWidgets[scriptId]);
		delete mRunningWidgets[scriptId];
		mRunningWidgets.remove(scriptId);
	}

	mRunningWidgets[scriptId] = new RunningWidget(fileName, *this);
	connect(&mBrick, SIGNAL(addedGraphicsWidget(trikControl::GraphicsWidget*))
	, mRunningWidgets[scriptId], SLOT(showGraphicsWidget(trikControl::GraphicsWidget*)));
	emit addRunningWidget(*mRunningWidgets[scriptId]);

	// After executing, a script will open a widget for painting with trikControl::Display.
	// This widget will get all keyboard events and we won't be able to abort execution at Power
	// key press. So, mRunningWidget should grab the keyboard input. Nevertheless, the script
	// can get keyboard events using trikControl::Keys class because it works directly
	// with the keyboard file.
	mRunningWidgets[scriptId]->grabKeyboard();
}

void Controller::directScriptExecutionStarted(int scriptId)
{
	scriptExecutionFromFileStarted(tr("direct command"), scriptId);
}
