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

#include <QsLog.h>
#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>

#include "updateWidget.h"

using namespace trikGui;

UpdateWidget::UpdateWidget(QWidget *parent)
	: TrikGuiDialog(parent)
{
	setWindowState(Qt::WindowFullScreen);

	mStatusLabel.setWordWrap(true);
	mStatusLabel.setAlignment(Qt::AlignCenter);
	mStatusLabel.setText(tr("Update is started..."));

	mLayout.addWidget(&mStatusLabel);

	mCancelButton = new QPushButton("Cancel");
	mLayout.addWidget(mCancelButton);
	mCancelButton->setDefault(true);

	setLayout(&mLayout);
	setFocusPolicy(Qt::StrongFocus);
}

UpdateWidget::~UpdateWidget()
{
}

void UpdateWidget::renewFocus()
{
}

QString UpdateWidget::menuEntry()
{
	return QString(tr("Update"));
}

void UpdateWidget::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
		case Qt::Key_Return: {
			cancelUpdating();
			break;
		}
		default: {
			TrikGuiDialog::keyPressEvent(event);
			break;
		}
	}
}

void UpdateWidget::cancelUpdating()
{
	mUpdateCommand.kill();
	mUpgradeCommand.kill();
}

void UpdateWidget::showStatus(QString const &text, bool isError)
{
	mStatusLabel.setAlignment(Qt::AlignCenter);
	if (isError) {
		mStatusLabel.setText(QString("<font color='red'>%1</font>").arg(text));
	} else {
		mStatusLabel.setText(QString("<font color='green'>%1</font>").arg(text));
	}

	update();
}

int UpdateWidget::exec()
{
	show();

	QEventLoop loop;
	QTimer::singleShot(1000, &loop, SLOT(quit()));
	loop.exec();

	QLOG_INFO() << "Running: " << "opkg update";
	qDebug() << "Running:" << "opkg update";
	mUpdateCommand.start("opkg update");
	bool update = mUpdateCommand.waitForFinished();

	if (update) {
		QLOG_INFO() << "Running: " << "opkg upgrade";
		qDebug() << "Running:" << "opkg upgrade";
		mUpgradeCommand.start("opkg upgrade");
		bool upgrade = mUpgradeCommand.waitForFinished(100000);

		if (upgrade) {
			showStatus(tr("Update and Upgrade is successfully finished"));
		} else {
			showStatus(tr("Upgrade is failed"), true);
		}
	} else {
		showStatus(tr("Update is failed"), true);
	}

	return TrikGuiDialog::exec();
}
