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
#include "../../../../sources/logging/stallsampler.h"

#include <catch2/catch.hpp>

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

/*
	The stall sampler: a stall made on purpose, a second spent in one
	function, and the report has to name that function - and not one that
	returned before the stall and left its address on the stack.
*/
namespace
{
	volatile double g_sink = 0;

		/// A stall of @a ms, all of it spent here
	__attribute__((noinline)) void stallFor(int ms)
	{
		QElapsedTimer timer;
		timer.start();
		while (timer.elapsed() < ms) {
			for (int i = 0 ; i < 1000 ; ++i) g_sink = g_sink * 1.0000001 + 1;
		}
	}

		/// Calls of itself, @a n deep, that leave their return
		/// addresses on the stack when they return
	__attribute__((noinline)) int returnedFrom(int n)
	{
		volatile int kept = n;
		return n ? returnedFrom(n - 1) + kept : 0;
	}

		/// A stall over the place returnedFrom() used: its frame holds
		/// what returnedFrom() left there, never written
	__attribute__((noinline)) void stallOverWhatReturned(int ms)
	{
		volatile char unwritten[3000];
		unwritten[0] = 1;
		stallFor(ms);
		g_sink = g_sink + unwritten[0];
	}

		/// The report of a stall of @a ms spent in @a stall
	QStringList reportOf(void (*stall)(int), int ms)
	{
		StallSampler::start();
		StallSampler::heartbeat();
		stall(ms);
		StallSampler::heartbeat();
		QStringList report;
		for (int i = 0 ; i < 40 && report.isEmpty() ; ++i)
		{
			QThread::msleep(25);
			StallSampler::heartbeat();
			report = StallSampler::takeReport();
		}
		StallSampler::stop();
		return report;
	}

		/// How many of the samples of @a report were in @a function:
		/// its bytes are those its unwinding data says, since functions
		/// lie one against the other and a guessed size takes the next
	int samplesIn(const QStringList &report, const void *function)
	{
	#if defined(Q_OS_WIN) && (defined(_M_X64) || defined(__x86_64__))
		DWORD64 image_base = 0;
		const PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(
				reinterpret_cast<DWORD64>(function), &image_base, nullptr);
		if (!entry) {
			return -1;
		}
		const QRegularExpression offset(QStringLiteral("\\+0x([0-9a-f]+) (\\d+)/(\\d+)"));
		int samples = 0;
		for (const QString &line : report)
		{
			const QRegularExpressionMatch match = offset.match(line);
			if (!match.hasMatch()) continue;
			const quintptr at = match.captured(1).toULongLong(nullptr, 16);
				//Each place of it the samples were at
			if (at >= entry->BeginAddress && at < entry->EndAddress) {
				samples += match.captured(2).toInt();
			}
		}
		return samples;
	#else
		Q_UNUSED(report) Q_UNUSED(function)
		return -1;
	#endif
	}

		/// The number of samples @a report says it took
	int samplesOf(const QStringList &report)
	{
		const QRegularExpression total(QStringLiteral("^StallSampler: (\\d+) samples"));
		const QRegularExpressionMatch match = total.match(report.value(0));
		return match.hasMatch() ? match.captured(1).toInt() : 0;
	}
}

TEST_CASE("The stall sampler names the function a stall went to", "[stallsampler]")
{
#if !defined(Q_OS_WIN) || !(defined(_M_X64) || defined(__x86_64__))
	SUCCEED("the sampler works on Windows x64 only");
#else
	const QStringList report = reportOf(stallFor, 1200);
	INFO(report.join(QLatin1Char('\n')).toStdString());
	REQUIRE(report.size() > 1);
	const int samples = samplesOf(report);
	REQUIRE(samples > 0);
		//In it, and in most of the samples
	CHECK(samplesIn(report, reinterpret_cast<const void *>(&stallFor)) * 2 >= samples);
#endif
}

TEST_CASE("The stall sampler does not name a function that already returned", "[stallsampler]")
{
#if !defined(Q_OS_WIN) || !(defined(_M_X64) || defined(__x86_64__))
	SUCCEED("the sampler works on Windows x64 only");
#else
		//What returnedFrom() leaves on the stack is under the frame of
		//the stall: read word by word, the stack names it in every sample
	g_sink = g_sink + returnedFrom(60);
	const QStringList report = reportOf(stallOverWhatReturned, 1200);
	INFO(report.join(QLatin1Char('\n')).toStdString());
	REQUIRE(samplesOf(report) > 0);
	CHECK(samplesIn(report, reinterpret_cast<const void *>(&stallOverWhatReturned)) * 2 >= samplesOf(report));
	CHECK(samplesIn(report, reinterpret_cast<const void *>(&returnedFrom)) == 0);
#endif
}
