/*******************************************************************************************************
 DkCentralWidget.cpp
 Created on:	14.11.2014
 
 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances
 
 Copyright (C) 2011-2013 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011-2013 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011-2013 Florian Kleber <florian@nomacs.org>

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 *******************************************************************************************************/

#include "DkCentralWidget.h"
#include "DkViewPort.h"
#include "DkMessageBox.h"
#include "DkThumbsWidgets.h"
#include "DkThumbs.h"
#include "DkBasicLoader.h"
#include "DkImageContainer.h"

#pragma warning(push, 0)	// no warnings from includes - begin
#include <QFileDialog>
#include <QClipboard>
#include <QStackedLayout>
#pragma warning(pop)		// no warnings from includes - end

namespace nmc {

DkTabInfo::DkTabInfo(const QSharedPointer<DkImageContainerT> imgC, int idx) {

	this->tabMode = tab_recent_files;
	this->imgC = imgC;
	this->tabIdx = idx;
}

bool DkTabInfo::operator ==(const DkTabInfo& o) const {

	return this->tabIdx == o.tabIdx;
}

void DkTabInfo::loadSettings(const QSettings& settings) {

	QFileInfo file = settings.value("tabFileInfo", "").toString();
	tabMode = settings.value("tabMode", tab_single_image).toInt();

	if (file.exists())
		imgC = QSharedPointer<DkImageContainerT>(new DkImageContainerT(file));
}

void DkTabInfo::saveSettings(QSettings& settings) const {

	if (imgC)
		settings.setValue("tabFileInfo", imgC->file().absoluteFilePath());
	settings.setValue("tabMode", tabMode);
}

void DkTabInfo::setFileInfo(const QFileInfo& fileInfo) {

	imgC = QSharedPointer<DkImageContainerT>(new DkImageContainerT(fileInfo));
}

QFileInfo DkTabInfo::getFileInfo() const {

	return (imgC) ? imgC->file() : QFileInfo();
}

void DkTabInfo::setTabIdx(int tabIdx) {

	this->tabIdx = tabIdx;
}

int DkTabInfo::getTabIdx() const {

	return tabIdx;
}

void DkTabInfo::setImage(QSharedPointer<DkImageContainerT> imgC) {
	
	this->imgC = imgC;
	tabMode = tab_single_image;
}

QSharedPointer<DkImageContainerT> DkTabInfo::getImage() const {

	return imgC;
}

QIcon DkTabInfo::getIcon() {
	
	QIcon icon;

	if (!imgC)
		return icon;

	if (tabMode == tab_thumb_preview)
		return QIcon(":/nomacs/img/thumbs-view.png");

	QSharedPointer<DkThumbNailT> thumb = imgC->getThumb();

	if (!thumb)
		return icon;

	QImage img = thumb->getImage();

	if (!img.isNull())
		icon = QPixmap::fromImage(img);

	return icon;
}

QString DkTabInfo::getTabText() const {

	QString tabText(QObject::tr("New Tab"));

	if (tabMode == tab_thumb_preview)
		return QObject::tr("Thumbnail Preview");

	if (imgC) {

		tabText = imgC->file().fileName();
		
		if (imgC->isEdited())
			tabText += "*";
	}

	return tabText;
}

int DkTabInfo::getMode() const {

	return tabMode;
}

void DkTabInfo::setMode(int mode) {

	this->tabMode = mode;
}

DkCentralWidget::DkCentralWidget(DkViewPort* viewport, QWidget* parent) : QWidget(parent) {

	this->viewport = viewport;
	setObjectName("DkCentralWidget");
	createLayout();
	loadSettings();

	setAcceptDrops(true);

	if (tabInfos.empty()) {
		DkTabInfo info;
		info.setMode(DkTabInfo::tab_empty);
		info.setTabIdx(0);
		addTab(info);
	}
}

DkCentralWidget::~DkCentralWidget() {
}

void DkCentralWidget::createLayout() {

	thumbScrollWidget = new DkThumbScrollWidget(this);
	thumbScrollWidget->getThumbWidget()->setBackgroundBrush(DkSettings::slideShow.backgroundColor);
	//thumbScrollWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	thumbScrollWidget->hide();

	tabbar = new QTabBar(this);
	tabbar->setShape(QTabBar::RoundedNorth);
	tabbar->setTabsClosable(true);
	tabbar->setMovable(true);
	tabbar->hide();
	//addTab(QFileInfo());

	widgets.resize(widget_end);
	widgets[viewport_widget] = viewport;
	widgets[thumbs_widget] = thumbScrollWidget;

	QWidget* viewWidget = new QWidget(this);
	viewLayout = new QStackedLayout(viewWidget);

	//for each (QWidget* w in widgets)
	for (QWidget* w : widgets)
		viewLayout->addWidget(w);

	QVBoxLayout* vbLayout = new QVBoxLayout(this);
	vbLayout->setContentsMargins(0,0,0,0);
	vbLayout->setSpacing(0);
	vbLayout->addWidget(tabbar);
	vbLayout->addWidget(viewWidget);

	recentFilesWidget = new DkRecentFilesWidget(viewWidget);
	// TODO: read the desktop size here...
	recentFilesWidget->setFixedSize(1920, 1080);	// TODO: this number will (hopefully : ) get old - bug for now WHXGA is enough
	
	// connections
	connect(this, SIGNAL(loadFileSignal(QFileInfo)), viewport, SLOT(loadFile(QFileInfo)));
	connect(viewport, SIGNAL(addTabSignal(const QFileInfo&)), this, SLOT(addTab(const QFileInfo&)));
	connect(viewport->getImageLoader(), SIGNAL(imageUpdatedSignal(QSharedPointer<DkImageContainerT>)), this, SLOT(imageLoaded(QSharedPointer<DkImageContainerT>)));

	connect(tabbar, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));
	connect(tabbar, SIGNAL(tabCloseRequested(int)), this, SLOT(tabCloseRequested(int)));
	connect(tabbar, SIGNAL(tabMoved(int, int)), this, SLOT(tabMoved(int, int)));

	// recent files widget
	connect(recentFilesWidget, SIGNAL(loadFileSignal(QFileInfo)), viewport, SLOT(loadFile(QFileInfo)));

	// thumbnail preview widget
	connect(thumbScrollWidget->getThumbWidget(), SIGNAL(loadFileSignal(QFileInfo)), viewport, SLOT(loadFile(QFileInfo)));
	connect(thumbScrollWidget, SIGNAL(updateDirSignal(QDir)), viewport->getImageLoader(), SLOT(loadDir(QDir)));
	connect(thumbScrollWidget->getThumbWidget(), SIGNAL(statusInfoSignal(QString, int)), this, SIGNAL(statusInfoSignal(QString, int)));

}

void DkCentralWidget::saveSettings(bool clearTabs) {

	if (tabInfos.size() <= 1)	// nothing to save here
		return;

	QSettings& settings = Settings::instance().getSettings();

	settings.beginGroup(objectName());
	settings.remove("Tabs");

	if (clearTabs) {

		settings.beginWriteArray("Tabs");

		for (int idx = 0; idx < tabInfos.size(); idx++) {
			settings.setArrayIndex(idx);
			tabInfos.at(idx).saveSettings(settings);
		}
		settings.endArray();
	}
	settings.endGroup();

}

void DkCentralWidget::loadSettings() {

	QSettings& settings = Settings::instance().getSettings();

	settings.beginGroup(objectName());

	int size = settings.beginReadArray("Tabs");
	for (int idx = 0; idx < size; idx++) {
		settings.setArrayIndex(idx);

		DkTabInfo tabInfo;
		tabInfo.loadSettings(settings);
		tabInfo.setTabIdx(idx);
		addTab(tabInfo);
	}

	settings.endArray();
	settings.endGroup();
}

DkViewPort* DkCentralWidget::getViewPort() const {

	return viewport;
}

DkThumbScrollWidget* DkCentralWidget::getThumbScrollWidget() const {

	return thumbScrollWidget;
}

DkRecentFilesWidget* DkCentralWidget::getRecentFilesWidget() const {

	return recentFilesWidget;
}

void DkCentralWidget::currentTabChanged(int idx) {

	if (idx < 0 && idx >= tabInfos.size())
		return;

	QSharedPointer<DkImageContainerT> imgC = tabInfos.at(idx).getImage();

	if (imgC && tabInfos.at(idx).getMode() == DkTabInfo::tab_single_image) {
		showViewPort(true);
	}
	else if (tabInfos.at(idx).getMode() == DkTabInfo::tab_thumb_preview) {
		showThumbView(true);
	}
	else if (tabInfos.at(idx).getMode() == DkTabInfo::tab_recent_files) {
		viewport->unloadImage();
		viewport->getImageLoader()->clearPath();
		viewport->setImage(QImage());
		showRecentFiles(true);
	}

	switchWidget(tabInfos.at(idx).getMode());
}

void DkCentralWidget::tabCloseRequested(int idx) {

	if (idx < 0 && idx >= tabInfos.size())
		return;

	removeTab(idx);
}

void DkCentralWidget::tabMoved(int from, int to) {

	DkTabInfo tabInfo = tabInfos.at(from);
	tabInfos.remove(from);
	tabInfos.insert(to, tabInfo);

	updateTabIdx();
}

void DkCentralWidget::addTab(const QFileInfo& fileInfo, int idx /* = -1 */) {

	QSharedPointer<DkImageContainerT> imgC = QSharedPointer<DkImageContainerT>(new DkImageContainerT(fileInfo));
	addTab(imgC, idx);
}

void DkCentralWidget::addTab(QSharedPointer<DkImageContainerT> imgC, int idx /* = -1 */) {

	if (idx == -1)
		idx = tabInfos.size();

	DkTabInfo tabInfo(imgC, idx);
	addTab(tabInfo);
}

void DkCentralWidget::addTab(const DkTabInfo& tabInfo) {

	tabInfos.push_back(tabInfo);
	tabbar->addTab(tabInfo.getTabText());
	tabbar->setCurrentIndex(tabInfo.getTabIdx());
	//tabbar->setTabButton(idx, QTabBar::ButtonPosition::RightSide, new DkButton(QPixmap(":/nomacs/img/close.png"), tr("Close")));

	if (tabInfos.size() > 1)
		tabbar->show();

	// TODO: add a plus button
	//// Create button what must be placed in tabs row
	//QToolButton* tb = new QToolButton();
	//tb->setText("+");
	//// Add empty, not enabled tab to tabWidget
	//tabbar->addTab("");
	//tabbar->setTabEnabled(0, false);
	//// Add tab button to current tab. Button will be enabled, but tab -- not
	//
	//tabbar->setTabButton(0, QTabBar::RightSide, tb);
}

void DkCentralWidget::removeTab(int tabIdx) {

	if (tabIdx == -1)
		tabIdx = tabbar->currentIndex();

	for (int idx = 0; idx < tabInfos.size(); idx++) {
		
		if (tabInfos.at(idx).getTabIdx() == tabIdx) {
			tabInfos.remove(idx);
			tabbar->removeTab(tabIdx);
		}
	}

	updateTabIdx();

	if (tabInfos.size() <= 1)
		tabbar->hide();
}

void DkCentralWidget::clearAllTabs() {
	
	for (int idx = 0; idx < tabInfos.size(); idx++)
		tabbar->removeTab(tabInfos.at(idx).getTabIdx());
	
	tabInfos.clear();

	tabbar->hide();
}

void DkCentralWidget::updateTab(DkTabInfo& tabInfo) {

	qDebug() << tabInfo.getTabText() << " set at tab location: " << tabInfo.getTabIdx();
	tabbar->setTabText(tabInfo.getTabIdx(), tabInfo.getTabText());
	tabbar->setTabIcon(tabInfo.getTabIdx(), tabInfo.getIcon());
}

void DkCentralWidget::updateTabIdx() {

	for (int idx = 0; idx < tabInfos.size(); idx++) {
		tabInfos[idx].setTabIdx(idx);
	}
}

void DkCentralWidget::nextTab() const {

	if (tabInfos.size() < 2)
		return;

	int idx = tabbar->currentIndex();
	idx++;
	idx %= tabInfos.size();
	tabbar->setCurrentIndex(idx);
}

void DkCentralWidget::previousTab() const {

	if (tabInfos.size() < 2)
		return;

	int idx = tabbar->currentIndex();
	idx--;
	if (idx < 0)
		idx = tabInfos.size()-1;
	tabbar->setCurrentIndex(idx);
}

void DkCentralWidget::imageLoaded(QSharedPointer<DkImageContainerT> img) {

	int idx = tabbar->currentIndex();

	if (idx == -1) {
		addTab(img, 0);
	}
	else if (idx > tabInfos.size())
		addTab(img, idx);
	else {
		DkTabInfo& tabInfo = tabInfos[idx];
		tabInfo.setImage(img);

		updateTab(tabInfo);
		switchWidget(tabInfo.getMode());
	}

	recentFilesWidget->hide();
}

QVector<DkTabInfo> DkCentralWidget::getTabs() const {

	return tabInfos;
}

void DkCentralWidget::showThumbView(bool show) {

	//if (show == thumbScrollWidget->isVisible())
	//	return;

	if (show) {

		
		DkTabInfo& tabInfo = tabInfos[tabbar->currentIndex()];
		tabInfo.setMode(DkTabInfo::tab_thumb_preview);

		// clear viewport
		viewport->unloadImage();
		viewport->getImageLoader()->clearPath();
		viewport->setImage(QImage());

		switchWidget(thumbs_widget);
		DkImageLoader* loader = viewport->getImageLoader();
		loader->setCurrentImage(tabInfo.getImage());
		thumbScrollWidget->updateThumbs(loader->getImages());
		//thumbScrollWidget->getThumbWidget()->updateLayout();
		connect(viewport->getImageLoader(), SIGNAL(updateDirSignal(QVector<QSharedPointer<DkImageContainerT> >)), thumbScrollWidget, SLOT(updateThumbs(QVector<QSharedPointer<DkImageContainerT> >)));
	}
	else {
		disconnect(viewport->getImageLoader(), SIGNAL(updateDirSignal(QVector<QSharedPointer<DkImageContainerT> >)), thumbScrollWidget, SLOT(updateThumbs(QVector<QSharedPointer<DkImageContainerT> >)));
		showViewPort(true);	// TODO: this triggers switchWidget - but switchWidget might also trigger showThumbView(false)
	}
}

void DkCentralWidget::showViewPort(bool show /* = true */) {

	if (show) {
		QSharedPointer<DkImageContainerT> imgC = tabInfos[tabbar->currentIndex()].getImage();
		if (imgC && imgC != viewport->getImageLoader()->getCurrentImage()) {
			viewport->loadImage(imgC);
		}
		else if (imgC && viewport->getImage().isNull()) {
			viewport->setImage(imgC->image());
		}
		else if (!imgC)
			viewport->getImageLoader()->firstFile();

		switchWidget(widgets[viewport_widget]);
	}
}

void DkCentralWidget::showRecentFiles(bool show) {

	if (show) {
		recentFilesWidget->setCustomStyle(!viewport->getImage().isNull() || thumbScrollWidget->isVisible());
		recentFilesWidget->show();
		qDebug() << "recent files size: " << recentFilesWidget->size();
	}
	else
		recentFilesWidget->hide();
}

void DkCentralWidget::showTabs(bool show) {

	if (show && tabInfos.size() > 1)
		tabbar->show();
	else
		tabbar->hide();
}

void DkCentralWidget::switchWidget(int widget) {

	if (widget == DkTabInfo::tab_single_image)
		switchWidget(widgets[viewport_widget]);
	else if (widget == DkTabInfo::tab_thumb_preview)
		switchWidget(widgets[thumbs_widget]);
	else
		qDebug() << "Sorry, I cannot switch to widget: " << widget;

	//recentFilesWidget->hide();
}

void DkCentralWidget::switchWidget(QWidget* widget) {

	if (viewLayout->currentWidget() == widget)
		return;

	if (widget)
		viewLayout->setCurrentWidget(widget);
	else
		viewLayout->setCurrentWidget(widgets[viewport_widget]);

	recentFilesWidget->hide();

	if (!tabInfos.isEmpty()) {
		
		int mode = widget == widgets[viewport_widget] ? DkTabInfo::tab_single_image : DkTabInfo::tab_thumb_preview;
		tabInfos[tabbar->currentIndex()].setMode(mode);
		updateTab(tabInfos[tabbar->currentIndex()]);
	}
}

int DkCentralWidget::currentViewMode() const {

	return tabInfos[tabbar->currentIndex()].getMode();
}

// DropEvents --------------------------------------------------------------------
void DkCentralWidget::dragEnterEvent(QDragEnterEvent *event) {

	printf("[DkCentralWidget] drag enter event\n");

	//if (event->source() == this)
	//	return;

	if (event->mimeData()->hasUrls()) {
		QUrl url = event->mimeData()->urls().at(0);

		QList<QUrl> urls = event->mimeData()->urls();

		for (int idx = 0; idx < urls.size(); idx++)
			qDebug() << "url: " << urls.at(idx);

		url = url.toLocalFile();

		// TODO: check if we accept appropriately (network drives that are not mounted)
		QFileInfo file = QFileInfo(url.toString());

		// just accept image files
		if (DkUtils::isValid(file))
			event->acceptProposedAction();
		else if (file.isDir())
			event->acceptProposedAction();
		else if (event->mimeData()->urls().at(0).isValid() && DkUtils::hasValidSuffix(event->mimeData()->urls().at(0).toString()))
			event->acceptProposedAction();

	}
	if (event->mimeData()->hasImage()) {
		event->acceptProposedAction();
	}

	QWidget::dragEnterEvent(event);
}

void DkCentralWidget::pasteImage() {

	qDebug() << "pasting...";

	QClipboard* clipboard = QApplication::clipboard();

	if (!loadFromMime(clipboard->mimeData()))
		viewport->getController()->setInfo("Clipboard has no image...");

}

void DkCentralWidget::dropEvent(QDropEvent *event) {

	if (event->source() == this) {
		event->accept();
		return;
	}

	if (!loadFromMime(event->mimeData()))
		viewport->getController()->setInfo(tr("Sorry, I could not drop the content."));
}

bool DkCentralWidget::loadFromMime(const QMimeData* mimeData) {

	if (!mimeData)
		return false;

	if (mimeData->hasUrls() && mimeData->urls().size() > 0 || mimeData->hasText()) {
		QUrl url = mimeData->hasText() ? QUrl::fromUserInput(mimeData->text()) : QUrl::fromUserInput(mimeData->urls().at(0).toString());
		qDebug() << "dropping: " << url;

		// TODO: toLocalFile() has problems with filenames that contain #
		QFileInfo file = QFileInfo(url.toLocalFile());
		QList<QUrl> urls = mimeData->urls();

		// merge OpenCV vec files if multiple vec files are dropped
		if (urls.size() > 1 && file.suffix() == "vec") {

			QVector<QFileInfo> vecFiles;

			for (int idx = 0; idx < urls.size(); idx++)
				vecFiles.append(urls.at(idx).toLocalFile());

			// ask user for filename
			QFileInfo sInfo(QFileDialog::getSaveFileName(this, tr("Save File"),
				vecFiles.at(0).absolutePath(), "Cascade Training File (*.vec)"));

			DkBasicLoader loader;
			int numFiles = loader.mergeVecFiles(vecFiles, sInfo);

			if (numFiles) {
				viewport->loadFile(sInfo);
				viewport->getController()->setInfo(tr("%1 vec files merged").arg(numFiles));
				return true;
			}

			return false;
		}
		else
			qDebug() << urls.size() << file.suffix() << " files dropped";

		if (tabInfos[tabbar->currentIndex()].getMode() == DkTabInfo::tab_thumb_preview) {
			// TODO: this event won't be called if the thumbs view is visible
			QDir newDir = (file.isDir()) ? QDir(file.absoluteFilePath()) : file.absolutePath();
			viewport->getImageLoader()->loadDir(newDir);
		}
		else {
			// just accept image files
			if (DkUtils::isValid(file) || file.isDir())
				viewport->loadFile(file);
			else if (url.isValid())
				viewport->getImageLoader()->downloadFile(url);
			else
				return false;
		}

		for (int idx = 1; idx < urls.size() && idx < 20; idx++)
			addTab(QFileInfo(urls[idx].toLocalFile()));

		return true;
	}
	else if (mimeData->hasImage()) {

		QImage dropImg = qvariant_cast<QImage>(mimeData->imageData());
		viewport->loadImage(dropImg);
		return true;
	}

	return false;
}

}