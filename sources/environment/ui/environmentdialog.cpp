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
#include "environmentdialog.h"

#include "../qetenvironment.h"

#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

/**
	@brief EnvironmentDialog::EnvironmentDialog
	@param parent
*/
EnvironmentDialog::EnvironmentDialog(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Environnement de travail"));
	resize(760, 560);
	buildWidgets();
	refreshContents();
}

/**
	@brief EnvironmentDialog::buildWidgets
*/
void EnvironmentDialog::buildWidgets()
{
	m_path = new QLineEdit(QETEnvironment::path(), this);
	m_browse = new QPushButton(tr("Parcourir…"), this);
	m_apply = new QPushButton(tr("Appliquer"), this);
	m_open = new QPushButton(tr("Ouvrir le dossier"), this);

	QHBoxLayout *path_layout = new QHBoxLayout();
	path_layout->addWidget(new QLabel(tr("Dossier"), this));
	path_layout->addWidget(m_path, 1);
	path_layout->addWidget(m_browse);
	path_layout->addWidget(m_apply);
	path_layout->addWidget(m_open);

	QLabel *explanation = new QLabel(
		tr("Tout est dans ce dossier : les projets, la bibliothèque de symboles, la "
		   "bibliothèque de l'entreprise, le catalogue de pièces, les cartouches et les "
		   "macros. Partager, c'est mettre ce dossier sur le lecteur réseau et pointer "
		   "chaque poste dessus — un seul chemin, et non six."), this);
	explanation->setWordWrap(true);

	QLabel *cautions = new QLabel(
		tr("<b>Ce qu'il faut savoir avant de le mettre en réseau :</b>"
		   "<ul>"
		   "<li>partager le <b>dossier entier</b>, jamais un sous-dossier seul ;</li>"
		   "<li>sur un lecteur synchronisé dans le nuage, marquer le dossier comme "
		   "<b>disponible localement</b> et non « en ligne uniquement » : sinon le "
		   "chargement à la demande cherche un fichier qui n'est pas là ;</li>"
		   "<li>tous les postes sur la <b>même version</b> du programme ;</li>"
		   "<li>réseau distant ou VPN dégrade les performances — mesurer avant d'adopter ;</li>"
		   "<li><b>fermer le programme en fin de journée</b> : une machine qui se met à "
		   "jour ou s'éteint avec un fichier ouvert corrompt ce fichier.</li>"
		   "</ul>"), this);
	cautions->setWordWrap(true);
	cautions->setTextFormat(Qt::RichText);

	m_contents = new QTextBrowser(this);

	m_backup = new QPushButton(tr("Copie de sauvegarde…"), this);
	m_backup->setToolTip(tr("Copie tout l'environnement dans un dossier choisi, qui doit être "
				"en dehors de l'environnement. C'est la protection de la "
				"bibliothèque et du catalogue, pas seulement des projets."));

	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	QDialogButtonBox *buttons = new QDialogButtonBox(this);
	buttons->addButton(m_backup, QDialogButtonBox::ActionRole);
	buttons->addButton(QDialogButtonBox::Close);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(path_layout);
	layout->addWidget(explanation);
	layout->addWidget(m_contents, 1);
	layout->addWidget(cautions);
	layout->addWidget(m_status);
	layout->addWidget(buttons);

	connect(m_browse, &QPushButton::clicked, this, &EnvironmentDialog::browse);
	connect(m_apply, &QPushButton::clicked, this, &EnvironmentDialog::apply);
	connect(m_open, &QPushButton::clicked, this, &EnvironmentDialog::openInFileManager);
	connect(m_backup, &QPushButton::clicked, this, &EnvironmentDialog::backup);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
}

/**
	@brief EnvironmentDialog::refreshContents
	Show what the environment holds, folder by folder, so that pointing at the
	wrong place is visible before anything is opened from it.
*/
void EnvironmentDialog::refreshContents()
{
	const QString root = QETEnvironment::path();
	const QDir directory(root);

	QString html = QStringLiteral("<h3>%1</h3>").arg(root.toHtmlEscaped());
	if (!directory.exists())
	{
		html += QStringLiteral("<p><b>%1</b></p>")
			.arg(tr("Ce dossier n'existe pas encore.").toHtmlEscaped());
		m_contents->setHtml(html);
		return;
	}

	html += QStringLiteral("<table cellspacing='6'><tr><th align='left'>%1</th>"
			       "<th align='left'>%2</th></tr>")
		.arg(tr("Contenu").toHtmlEscaped(), tr("Fichiers").toHtmlEscaped());

	const QStringList folders = QETEnvironment::skeletonFolders();
	for (const QString &folder : folders)
	{
		const QString absolute = directory.absoluteFilePath(folder);
		int count = 0;
		if (QFileInfo::exists(absolute))
		{
			QDirIterator iterator(absolute, QDir::Files | QDir::NoDotAndDotDot,
					      QDirIterator::Subdirectories);
			while (iterator.hasNext())
			{
				iterator.next();
				++count;
			}
		}
		html += QStringLiteral("<tr><td>%1</td><td>%2</td></tr>")
			.arg(folder.toHtmlEscaped(),
			     QFileInfo::exists(absolute) ? QString::number(count)
							 : tr("absent").toHtmlEscaped());
	}

	const QString catalog = QETEnvironment::catalogFile();
	const QFileInfo catalog_info(catalog);
	html += QStringLiteral("<tr><td>catalog.sqlite</td><td>%1</td></tr>")
		.arg(catalog_info.exists()
		     ? tr("%1 Ko").arg(catalog_info.size() / 1024).toHtmlEscaped()
		     : tr("absent").toHtmlEscaped());
	html += QStringLiteral("</table>");

	if (!QFileInfo(root).isWritable())
	{
		html += QStringLiteral("<p><b>%1</b></p>")
			.arg(tr("Ce dossier est en lecture seule pour cet utilisateur : le dessin "
				"fonctionne, l'enregistrement non.").toHtmlEscaped());
	}

	m_contents->setHtml(html);
}

/**
	@brief EnvironmentDialog::browse
*/
void EnvironmentDialog::browse()
{
	const QString chosen = QFileDialog::getExistingDirectory(
		this, tr("Choisir le dossier d'environnement"), m_path->text());
	if (!chosen.isEmpty()) {
		m_path->setText(chosen);
	}
}

/**
	@brief EnvironmentDialog::apply
*/
void EnvironmentDialog::apply()
{
	const QString wanted = m_path->text().trimmed();
	if (wanted == QETEnvironment::path())
	{
		m_status->setText(tr("C'est déjà l'environnement en cours."));
		return;
	}

	// An existing folder that carries nothing of an environment is almost
	// always the wrong folder. Say it, and let the user insist.
	if (QFileInfo::exists(wanted) && !QETEnvironment::looksLikeEnvironment(wanted))
	{
		if (QMessageBox::question(this, tr("Dossier inattendu"),
					  tr("%1 ne ressemble pas à un environnement : les dossiers "
					     "habituels n'y sont pas. Les créer et l'utiliser quand "
					     "même ?").arg(wanted))
		    != QMessageBox::Yes)
		{
			return;
		}
	}

	QString error;
	if (!QETEnvironment::setPath(wanted, &error))
	{
		QMessageBox::warning(this, tr("Environnement inchangé"), error);
		return;
	}

	m_path_changed = true;
	refreshContents();

	QString message = tr("Environnement changé pour %1. Fermez et relancez le programme : "
			     "la bibliothèque et le catalogue déjà chargés viennent de l'ancien.")
			  .arg(wanted);
	if (!error.isEmpty()) {
		message += QStringLiteral("\n\n") + error;
	}
	QMessageBox::information(this, tr("Redémarrage nécessaire"), message);
	m_status->setText(message);
}

/**
	@brief EnvironmentDialog::backup
*/
void EnvironmentDialog::backup()
{
	const QString chosen = QFileDialog::getExistingDirectory(
		this, tr("Où écrire la copie de sauvegarde"));
	if (chosen.isEmpty()) {
		return;
	}

	// The date in the name so that a copy never silently replaces the copy of
	// another day - a backup that overwrites itself is one backup, not a history.
	const QString destination = QDir(chosen).absoluteFilePath(
		QStringLiteral("environnement-") + QDate::currentDate().toString(Qt::ISODate));

	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	QString error;
	const int copied = QETEnvironment::copyTo(destination, &error);
	QGuiApplication::restoreOverrideCursor();

	if (copied < 0)
	{
		QMessageBox::warning(this, tr("Copie non effectuée"), error);
		return;
	}

	const QString message = tr("%n fichier(s) copié(s) dans %1.", "", copied).arg(destination);
	m_status->setText(message);
	QMessageBox::information(this, tr("Copie de sauvegarde effectuée"), message);
}

/**
	@brief EnvironmentDialog::openInFileManager
*/
void EnvironmentDialog::openInFileManager()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(QETEnvironment::path()));
}

/**
	@brief EnvironmentDialog::pathChanged
	@return true when the user changed the path
*/
bool EnvironmentDialog::pathChanged() const
{
	return m_path_changed;
}
