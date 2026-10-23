/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef STALLSAMPLER_H
#define STALLSAMPLER_H

#include <QStringList>

/**
	@brief The StallSampler namespace
	What the main thread is doing while it stalls, so that the line the
	EventLoopWatchdog writes says where the time went and not only how
	much of it.

	A thread of its own looks every 50 ms at the last tick of the main
	thread. When the main thread has not ticked for 300 ms, it is suspended
	a moment, the top of its stack is copied, and it is resumed: a copy of
	memory and nothing else while it is stopped, since anything that takes
	a lock there could wait for a lock the stopped thread holds. The copy is
	then unwound, frame by frame, by the unwinding data every x64 module
	carries, and each return address that falls inside the program is
	counted once per sample. When the stall ends, the most counted are the
	report: the functions the stall was spent in, as offsets into the
	program for a symbolizer (addr2line) run against the same build.

	Unwound, and not read word by word: the words of a stack hold the
	return addresses of calls that already returned too, and functions the
	stall never went through showed in a quarter of the samples
	(23/10/2026).

	Windows only; elsewhere the functions do nothing.
*/
namespace StallSampler
{
		/// Start watching the calling thread, which must be the main one
	void start();
		/// The main thread is alive: called by each tick of the watchdog
	void heartbeat();
		/// The lines that say where the last stall went, emptied by the call
	QStringList takeReport();
		/// Stop watching, for a test that started it
	void stop();
}

#endif // STALLSAMPLER_H
