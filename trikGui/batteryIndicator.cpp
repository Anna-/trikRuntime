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

#include "batteryIndicator.h"

#include <QtCore/QString>

using namespace trikGui;

BatteryIndicator::BatteryIndicator(trikControl::Brick &brick, QWidget *parent)
	: QLabel(parent)
	, mBrick(brick)
{
	renew();

	mRenewTimer.setInterval(mRenewInterval);
	mRenewTimer.setSingleShot(false);
	connect(&mRenewTimer, SIGNAL(timeout()), this, SLOT(renew()));
	mRenewTimer.start();
}

void BatteryIndicator::renew()
{
	setText(QString::number(mBrick.battery()->readVoltage(), 'f', 1) + " V");
}
