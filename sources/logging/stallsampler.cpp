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
#include "stallsampler.h"

#if defined(Q_OS_WIN) && (defined(_M_X64) || defined(__x86_64__))

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

namespace
{
		/// How often the sampler looks, and samples during a stall
	constexpr unsigned long period_ms = 50;
		/// How long without a tick is a stall
	constexpr qint64 stall_ms = 300;
		/// How much of the top of the stack is copied
	constexpr size_t copy_bytes = 256 * 1024;
		/// Zeros after the copy: the unwinding of a frame cut by the end
		/// of the copy reads them, and not past the buffer
	constexpr size_t slack_bytes = 64 * 1024;
		/// How many frames of a sample are followed
	constexpr int max_frames = 400;
		/// How many addresses the report names
	constexpr int reported = 120;

		/// What one address of the program was in the samples of a stall:
		/// how many held it, and how many frames from the top of the stack
		/// it was, summed - the nearer the top, the nearer what the stall
		/// was doing
	struct Seen
	{
		int samples = 0;
		qint64 depth = 0;
	};

		/**
			The frames of a stack, the running one first, unwound from
			@a context by the unwinding data every x64 module carries - on
			@a copy, the bytes from @a top, and not on the stack itself:
			the thread runs again meanwhile. A register that points into
			the stack is pointed into the copy before each frame, so that
			what the unwinding reads is read there.
			@return the number of frames written to @a frames
		*/
	int unwind(CONTEXT context, quintptr top, const quintptr *copy, size_t bytes,
		   quintptr *frames, int max)
	{
		const quintptr base = reinterpret_cast<quintptr>(copy);
		const auto intoCopy = [top, bytes, base](DWORD64 &value) {
			if (value >= top && value < top + bytes) value = value - top + base;
		};
		int count = 0;
		while (count < max && context.Rip)
		{
			frames[count++] = context.Rip;
			for (DWORD64 *value : {&context.Rsp, &context.Rbp, &context.Rbx, &context.Rsi,
					       &context.Rdi, &context.R12, &context.R13, &context.R14,
					       &context.R15, &context.Rax, &context.Rcx, &context.Rdx,
					       &context.R8, &context.R9, &context.R10, &context.R11}) {
				intoCopy(*value);
			}
			if (context.Rsp < base || context.Rsp >= base + bytes) {
				break;
			}
			DWORD64 image_base = 0;
			PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
			if (entry)
			{
				PVOID handler_data = nullptr;
				DWORD64 establisher = 0;
				RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, entry, &context,
						 &handler_data, &establisher, nullptr);
			}
			else
			{
					//A function that calls nothing: its return address
					//on the top of the stack
				context.Rip = *reinterpret_cast<const DWORD64 *>(context.Rsp);
				context.Rsp += sizeof(DWORD64);
			}
			if (context.Rsp < base || context.Rsp >= base + bytes) {
				break;
			}
		}
		return count;
	}

	QElapsedTimer g_clock;
	std::atomic<qint64> g_last_tick{0};
	HANDLE g_main = nullptr;
	quintptr g_stack_low = 0;
	quintptr g_stack_high = 0;
	quintptr g_image_low = 0;
	quintptr g_image_high = 0;
	QMutex g_mutex;
	QStringList g_report;

	class Sampler : public QThread
	{
		protected:
			void run() override
			{
				std::vector<quintptr> copy((copy_bytes + slack_bytes) / sizeof(quintptr));
				QHash<quintptr, Seen> counts;
				int samples = 0;
				qint64 since = -1;
				while (!isInterruptionRequested())
				{
					msleep(period_ms);
					const qint64 now = g_clock.elapsed();
					const qint64 last = g_last_tick.load();
					if (now - last > stall_ms)
					{
						if (since < 0) since = last;
						if (sample(copy, counts)) ++samples;
					}
					else if (samples)
					{
						report(counts, samples, last - since);
						counts.clear();
						samples = 0;
						since = -1;
					}
				}
			}

		private:
				/// The main thread stopped, the top of its stack copied, and
				/// started again: nothing but a copy while it is stopped
			bool sample(std::vector<quintptr> &copy, QHash<quintptr, Seen> &counts)
			{
				if (SuspendThread(g_main) == DWORD(-1)) {
					return false;
				}
				CONTEXT context;
				std::memset(&context, 0, sizeof(context));
				context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
				size_t bytes = 0;
				quintptr top = 0;
				if (GetThreadContext(g_main, &context))
				{
					top = context.Rsp;
					if (top >= g_stack_low && top < g_stack_high)
					{
						bytes = std::min<size_t>(g_stack_high - top, copy_bytes);
						std::memcpy(copy.data(), reinterpret_cast<const void *>(top), bytes);
					}
				}
				ResumeThread(g_main);
				if (!bytes) {
					return false;
				}
				std::memset(reinterpret_cast<char *>(copy.data()) + bytes, 0,
					    copy.size() * sizeof(quintptr) - bytes);

					//Each address of the program once per sample, at the
					//frame nearest the top
				quintptr frames[max_frames];
				const int count = unwind(context, top, copy.data(), bytes, frames, max_frames);
				QHash<quintptr, qint64> seen;
				for (int i = 0 ; i < count ; ++i)
				{
					const quintptr address = frames[i];
					if (address >= g_image_low && address < g_image_high && !seen.contains(address)) {
						seen.insert(address, i);
					}
				}
				for (auto it = seen.cbegin() ; it != seen.cend() ; ++it)
				{
					Seen &entry = counts[it.key()];
					++entry.samples;
					entry.depth += it.value();
				}
				return true;
			}

			void report(const QHash<quintptr, Seen> &counts, int samples, qint64 ms)
			{
					//The most sampled first; of as many, the nearest the top
					//of the stack: the start of the program is in every
					//sample too, at the bottom
				struct Line { int samples; qint64 depth; quintptr address; };
				QList<Line> lines_of;
				for (auto it = counts.cbegin() ; it != counts.cend() ; ++it) {
					lines_of << Line{it.value().samples, it.value().depth / it.value().samples, it.key()};
				}
				std::sort(lines_of.begin(), lines_of.end(), [](const Line &a, const Line &b) {
					if (a.samples != b.samples) return a.samples > b.samples;
					return a.depth < b.depth;
				});
				QStringList lines;
				lines << QStringLiteral("StallSampler: %1 samples over %2 ms; offsets into the program, "
							"most sampled first:").arg(samples).arg(ms);
				for (int i = 0 ; i < lines_of.size() && i < reported ; ++i)
				{
					lines << QStringLiteral("StallSampler:   +0x%1 %2/%3 d%4")
							 .arg(lines_of.at(i).address - g_image_low, 0, 16)
							 .arg(lines_of.at(i).samples).arg(samples)
							 .arg(lines_of.at(i).depth);
				}
				QMutexLocker locker(&g_mutex);
				g_report = lines;
			}
	};

	Sampler *g_sampler = nullptr;
}

void StallSampler::start()
{
	if (g_sampler) {
		return;
	}
	if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main,
			     THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
			     FALSE, 0)) {
		return;
	}
		//The stack of the main thread, from its thread information block
	const NT_TIB *tib = reinterpret_cast<const NT_TIB *>(NtCurrentTeb());
	g_stack_low = reinterpret_cast<quintptr>(tib->StackLimit);
	g_stack_high = reinterpret_cast<quintptr>(tib->StackBase);
		//The program in memory, from its own header
	const auto *image = reinterpret_cast<const unsigned char *>(GetModuleHandleW(nullptr));
	const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(image);
	const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(image + dos->e_lfanew);
	g_image_low = reinterpret_cast<quintptr>(image);
	g_image_high = g_image_low + nt->OptionalHeader.SizeOfImage;

	g_clock.start();
	g_last_tick = 0;
	g_sampler = new Sampler;
	g_sampler->start();
}

void StallSampler::heartbeat()
{
	if (g_sampler) {
		g_last_tick = g_clock.elapsed();
	}
}

QStringList StallSampler::takeReport()
{
	QMutexLocker locker(&g_mutex);
	QStringList report;
	report.swap(g_report);
	return report;
}

void StallSampler::stop()
{
	if (!g_sampler) {
		return;
	}
	g_sampler->requestInterruption();
	g_sampler->wait();
	delete g_sampler;
	g_sampler = nullptr;
	CloseHandle(g_main);
	g_main = nullptr;
}

#else

void StallSampler::start() {}
void StallSampler::heartbeat() {}
QStringList StallSampler::takeReport() {return {};}
void StallSampler::stop() {}

#endif
