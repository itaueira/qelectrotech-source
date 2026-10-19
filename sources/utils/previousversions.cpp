/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.
*/
#include "previousversions.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
const QString CountKey = QStringLiteral("diagrameditor/previous-versions");
constexpr int DefaultCount = 5;

	//setRoot(), for this run only
QString root_override;

	//What a version of @p info is named, without the extension: its base
	//name and a time
QString versionPrefix(const QFileInfo &info)
{
	return info.completeBaseName() + QLatin1Char(' ');
}

QByteArray digest(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) return {};
	QCryptographicHash hash(QCryptographicHash::Sha256);
	hash.addData(&file);
	return hash.result();
}

	//Whether @p folder is @p parent or inside it; on Windows the case of
	//the letters does not tell two folders apart
bool inside(const QString &folder, const QString &parent)
{
	const QString a = QDir::cleanPath(folder);
	const QString b = QDir::cleanPath(parent);
	if (a.isEmpty() || b.isEmpty()) return false;
#ifdef Q_OS_WIN
	const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
	const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
	return a.compare(b, cs) == 0
			|| a.startsWith(b.endsWith(QLatin1Char('/')) ? b : b + QLatin1Char('/'), cs);
}

	//Whether @p file_path is in the temporary folder of the system, written
	//as it was given or with its links resolved
bool isTemporary(const QString &file_path)
{
	const QString folder = QFileInfo(file_path).absolutePath();
	const QString temp = QDir::tempPath();
	if (inside(folder, temp)) return true;
	return inside(QFileInfo(folder).canonicalFilePath(), QFileInfo(temp).canonicalFilePath());
}
}

int PreviousVersions::count()
{
	return qMax(0, QSettings().value(CountKey, DefaultCount).toInt());
}

void PreviousVersions::setCount(int count)
{
	QSettings().setValue(CountKey, qMax(0, count));
}

QString PreviousVersions::root()
{
	if (!root_override.isEmpty()) return root_override;
		//A test that runs the application in the test mode of QStandardPaths
		//is promised that it reads and writes nothing of the user's; the
		//documents are not among the locations that mode moves, the data of
		//the application is.
	if (QStandardPaths::isTestModeEnabled())
		return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
				.filePath(QStringLiteral("Previous versions"));
	return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
			.filePath(QStringLiteral("QElectroTech/Previous versions"));
}

void PreviousVersions::setRoot(const QString &root)
{
	root_override = root;
}

QString PreviousVersions::folder(const QString &file_path)
{
	const QFileInfo info(file_path);
		//The folder the project is in, the way it was written does not
		//count: "C:/a/b", "C:\\a\\b" and, on Windows, "c:/A/b" are one folder
	QString where = QDir::cleanPath(info.absolutePath());
#ifdef Q_OS_WIN
	where = where.toLower();
#endif
	const QString code = QString::fromLatin1(
				QCryptographicHash::hash(where.toUtf8(), QCryptographicHash::Sha1)
				.toHex().left(8));
	return QDir(root()).filePath(QStringLiteral("%1 (%2)").arg(info.completeBaseName(), code));
}

QStringList PreviousVersions::kept(const QString &file_path)
{
	const QFileInfo info(file_path);
	const QDir dir(folder(file_path));
		//Each copy has the time its version was saved (keep()); a .qet and
		//a .qetz of the same project are versions of it alike
		//"????-??-?? ??-??-??" written with "\?", so that the compiler doesn't
		//see the trigraph "??-" in it (-Wtrigraphs)
	const QString stamp_ = QStringLiteral("?\?\?\?-?\?-?\? ?\?-?\?-?\?");
	QStringList names = dir.entryList({versionPrefix(info) + stamp_ + QStringLiteral("*.qet"),
									   versionPrefix(info) + stamp_ + QStringLiteral("*.qetz")},
									  QDir::Files, QDir::Time);
	QStringList paths;
	for (const QString &name : std::as_const(names))
		paths << dir.filePath(name);
	return paths;
}

bool PreviousVersions::keep(const QString &file_path, int count, QString *error)
{
	const QFileInfo info(file_path);
	if (count <= 0 || !info.isFile()) return true;
		//A project in the temporary folder of the system is a copy that
		//something else owns - a test, an attachment opened from a mail -
		//and its versions would pile up in the user's documents under names
		//nobody knows: each run of the tests that save a project left a
		//folder there. Kept all the same when setRoot() chose where the
		//versions go, which is what a test of this very feature does.
	if (root_override.isEmpty() && isTemporary(file_path)) return true;

	const QStringList versions = kept(file_path);
	if (!versions.isEmpty() && QFileInfo(versions.first()).size() == info.size()
			&& digest(versions.first()) == digest(file_path))
		return true;

	QDir dir(folder(file_path));
	if (!dir.exists() && !QDir().mkpath(dir.path())) {
		if (error) *error = QStringLiteral("cannot create the folder %1").arg(dir.path());
		return false;
	}
		//Named by the time this version was saved, made unique if another
		//was saved the same second
	const QDateTime saved = info.lastModified();
	const QString stem = versionPrefix(info) + saved.toString(QStringLiteral("yyyy-MM-dd HH-mm-ss"));
	QString target = dir.filePath(stem + QLatin1Char('.') + info.suffix());
	for (int n = 2 ; QFileInfo::exists(target) ; ++n)
		target = dir.filePath(stem + QStringLiteral(" (%1).").arg(n) + info.suffix());
	if (!QFile::copy(file_path, target)) {
		if (error) *error = QStringLiteral("cannot copy %1 to %2").arg(file_path, target);
		return false;
	}
	QFile copy(target);
	if (copy.open(QIODevice::ReadWrite))
		copy.setFileTime(saved, QFileDevice::FileModificationTime);

	const QStringList all = kept(file_path);
	for (int i = count ; i < all.size() ; ++i)
		QFile::remove(all.at(i));
	return true;
}
