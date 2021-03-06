/* Copyright 2014 Roman Kurbatov
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

#include "motorLever.h"

#include <QtGui/QKeyEvent>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	#include <QtGui/QStylePainter>
	#include <QtGui/QStyleOptionFocusRect>
#else
	#include <QtWidgets/QStylePainter>
	#include <QtWidgets/QStyleOptionFocusRect>
#endif

#include <trikControl/motor.h>

using namespace trikGui;

MotorLever::MotorLever(QString const &port, trikControl::Motor &motor, QWidget *parent)
	: QWidget(parent)
	, mMotor(motor)
	, mIsOn(false)
	, mMaxPower(100)
	, mMinPower(-100)
	, mPowerStep(10)
	, mPower(0)
	, mNameLabel(port)
	, mPowerLabel("0")
	, mOnOffLabel(tr("off"))
{
	mMotor.powerOff();

	mPowerBar.setOrientation(Qt::Horizontal);
	mPowerBar.setMinimum(mMinPower);
	mPowerBar.setMaximum(mMaxPower);
	mPowerBar.setValue(0);
	mPowerBar.setTextVisible(false);

	mNameLabel.setAlignment(Qt::AlignCenter);
	mPowerBar.setAlignment(Qt::AlignCenter);
	mPowerLabel.setAlignment(Qt::AlignCenter);
	mOnOffLabel.setAlignment(Qt::AlignCenter);

	// mPowerLabel and mOnOffLabel can change their widths during work. It will cause mPowerBar
	// width change. To prevent it, we set fixed widths for mPowerLabel and mOnOffLabel.
	// They are equal to maximum widths of the widgets. For mPowerLabel it is when the label text
	// is "-100", for mOnOffLabel - "off".
	mPowerLabel.setFixedWidth(40);
	mOnOffLabel.setFixedWidth(48);

	mLayout.addWidget(&mNameLabel);
	mLayout.addWidget(&mPowerBar);
	mLayout.addWidget(&mPowerLabel);
	mLayout.addWidget(&mOnOffLabel);
	setLayout(&mLayout);

	setFocusPolicy(Qt::StrongFocus);
}

MotorLever::~MotorLever()
{
	mMotor.powerOff();
}

void MotorLever::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
		case Qt::Key_Right: {
			setPower(mPower + mPowerStep);
			break;
		}
		case Qt::Key_Left: {
			setPower(mPower - mPowerStep);
			break;
		}
		case Qt::Key_Return: {
			turnOnOff();
			break;
		}
		default: {
			QWidget::keyPressEvent(event);
		}
	}
}

void MotorLever::paintEvent(QPaintEvent *)
{
	QStylePainter painter(this);

	if (hasFocus()) {
		QStyleOptionFocusRect option;
		option.initFrom(this);
		option.backgroundColor = palette().color(QPalette::Background);

		painter.drawPrimitive(QStyle::PE_FrameFocusRect, option);
	}
}

void MotorLever::setPower(int power)
{
	if (power > mMaxPower || power < mMinPower) {
		return;
	}

	mPower = power;
	mPowerBar.setValue(power);
	mPowerLabel.setText(QString::number(power));
	if (mIsOn) {
		mMotor.setPower(power);
	}
}

void MotorLever::turnOnOff()
{
	mIsOn = !mIsOn;
	if (mIsOn) {
		mOnOffLabel.setText(tr("on"));
		mMotor.setPower(mPower);
	} else {
		mOnOffLabel.setText(tr("off"));
		mMotor.powerOff();
	}
}
