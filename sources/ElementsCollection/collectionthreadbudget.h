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
#ifndef COLLECTIONTHREADBUDGET_H
#define COLLECTIONTHREADBUDGET_H

#include <QString>

/**
	@brief How many worker threads the elements collection scan may use.

	Walking the collection parses one XML file per symbol - thousands of
	them - and it is spread over the thread pool, which by default holds one
	thread per core. While that runs, every core is busy, and the thread
	drawing the window is one runnable thread too many: it gets the machine
	back only when the scheduler takes a core away from a worker. The window
	is not blocked - it is queued, which looks the same to whoever is
	waiting in front of it.

	So the scan gives cores back. Not to be polite: to keep the one thread
	the user can see out of a queue it has no reason to be in.

	This is a trade, and it is worth saying which way: the scan gets
	*slower* in wall-clock time, because it has fewer cores. What it buys is
	that everything else stays quick while it runs. A cap set too low pays
	far more than it buys - the panel would stay empty for longer with
	nothing to show for it - which is why the number below is measured and
	not argued.

	The rule lives alone here, without the pool it is meant for, so it can
	be checked without threads: the machine's core count goes in, the number
	of workers comes out.
*/
class CollectionThreadBudget
{
	public:
		/**
			Cores this scan leaves to the rest of the machine.

			Measured, not guessed, on an eight-core machine: a thread that
			wants the processor continuously - which is what the thread
			drawing a folio does, and what a timer waking up every 10ms
			does not - was timed against itself while the scan ran. Median
			of five runs per arm, arms alternated round by round, baseline
			taken inside each run so that the machine warming up over the
			battery could not land on one arm:

			  cores left free   scan takes   that thread is slowed by
			        0             +0.0%              x2.46
			        1             +1.0%              x2.23
			        2             +2.9%              x1.87
			        4            +11.6%              x1.77
			        6            +53.3%              x1.69

			Two is the knee. Leaving one free buys little - the scan still
			has a worker for every core the rest of the machine wants -
			while leaving two removes 40% of the interference for under 3%
			more scan time. Below that the curve flattens and the price
			does not: the last row costs half again as much scan for a
			gain that is inside the noise of the row above it.
		*/
		static constexpr int kCoresLeftFree = 2;

		/**
			Name of the environment variable that overrides the count, so
			that the cap can be re-measured on a machine with another core
			count without rebuilding. Not a preference: nothing in the
			interface sets it, and an unset or unreadable value simply
			leaves the rule below in charge.
		*/
		static const char *const kOverrideVariable;

		static int threadCount(int machine_cores);
		static int threadCount(int machine_cores, const QString &requested);

	private:
		CollectionThreadBudget() = delete;
};

#endif // COLLECTIONTHREADBUDGET_H
