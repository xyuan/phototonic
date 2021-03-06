/*
 *  Copyright (C) 2013 Ofer Kashayov <oferkv@live.com>
 *  This file is part of Phototonic Image Viewer.
 *
 *  Phototonic is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Phototonic is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Phototonic.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "thumbview.h"

ThumbView::ThumbView(QWidget *parent) : QListView(parent)
{
	GData::thumbsBackgroundColor = GData::appSettings->value("backgroundThumbColor").value<QColor>();
	GData::thumbsTextColor = GData::appSettings->value("textThumbColor").value<QColor>();
	setThumbColors();
	GData::thumbSpacing = GData::appSettings->value("thumbSpacing").toInt();
	GData::thumbsLayout = GData::appSettings->value("thumbLayout").toInt();
	thumbSize = GData::appSettings->value("thumbsZoomVal").toInt();
	currentRow = 0;

	setViewMode(QListView::IconMode);
	setSelectionMode(QAbstractItemView::ExtendedSelection);
	setResizeMode(QListView::Adjust);
	setFrameShape(QFrame::NoFrame);
	setMovement(QListView::Movement(QListView::Static));
	setWordWrap(true);
	setDragEnabled(true);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setItemDelegate(new QItemDelegate);

	thumbViewModel = new QStandardItemModel(this);
	thumbViewModel->setSortRole(SortRole);
	thumbIsLoaded = new QList<bool>();
	setModel(thumbViewModel);

	connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(updateIndex()));
	connect(this->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), 
				this, SLOT(handleSelectionChanged(QItemSelection)));
   	connect(this, SIGNAL(doubleClicked(const QModelIndex &)), 
				parent, SLOT(loadImagefromThumb(const QModelIndex &)));

	thumbsDir = new QDir();
	QStringList *fileFilters = new QStringList;
	*fileFilters << "*.BMP" << "*.GIF" << "*.JPG" << "*.JPEG" << "*.JPE" << "*.PNG"
				 << "*.PBM" << "*.PGM" << "*.PPM" << "*.XBM" << "*.XPM" << "*.SVG"
				 << "*.TIFF" << "*.TIF" << "*.TGA";
	thumbsDir->setFilter(QDir::Files);
	thumbsDir->setNameFilters(*fileFilters);

	emptyImg.load(":/images/no_image.png");

	QTime time = QTime::currentTime();
	qsrand((uint)time.msec());
	mainWindow = parent;
}

void ThumbView::setThumbColors()
{
	QPalette scrollBarOrigPal = verticalScrollBar()->palette();
	QPalette thumbViewOrigPal = palette();
	thumbViewOrigPal.setColor(QPalette::Base, GData::thumbsBackgroundColor);
	thumbViewOrigPal.setColor(QPalette::Text, GData::thumbsTextColor);
	setPalette(thumbViewOrigPal);
	verticalScrollBar()->setPalette(scrollBarOrigPal);
}

void ThumbView::selectCurrentIndex()
{
	setCurrentIndex(currentIndex);
}

QString ThumbView::getSingleSelectionFilename()
{
	if (selectionModel()->selectedIndexes().size() == 1)
		return thumbViewModel->item(selectionModel()->selectedIndexes().first().row())->data(FileNameRole).toString();

	return("");
}

int ThumbView::getNextRow()
{
	if (currentRow == thumbViewModel->rowCount() - 2)
		return currentRow;

	return currentRow + 1;
}

int ThumbView::getPrevRow()
{
	if (!currentRow)
		return 0;

	return currentRow - 1;
}

int ThumbView::getLastRow()
{
	return thumbViewModel->rowCount() - 2;
}

int ThumbView::getRandomRow()
{
	return qrand() % (thumbViewModel->rowCount() - 1);
}

int ThumbView::getCurrentRow()
{
	return currentRow;
}

void ThumbView::setCurrentRow(int row)
{
	if (row >= 0)
		currentRow = row;
	else
		currentRow = 0;

	QString title = thumbViewModel->item(currentRow)->data(FileNameRole).toString()
			+ " - ["
			+ QString::number(currentRow + 1)
			+ "/"
			+ QString::number(thumbViewModel->rowCount() - 1)
			+ "] - Phototonic";
	mainWindow->setWindowTitle(title);
}

void ThumbView::setCurrentIndexByName(QString &FileName)
{
	QModelIndexList indexList = thumbViewModel->match(thumbViewModel->index(0, 0), FileNameRole, FileName);
	if (indexList.size())
	{
	 	currentIndex = indexList[0];
	 	setCurrentRow(currentIndex.row());
 	}
}

void ThumbView::handleSelectionChanged(const QItemSelection&)
{
	QString info;
	QString state;
	QModelIndexList indexesList = selectionModel()->selectedIndexes();
	int nSelected = indexesList.size();
	
	if (!nSelected)
		state = QString::number(thumbFileInfoList.size()) + " images";
	else if (nSelected > 1)
		state = QString("Selected " + QString::number(nSelected) + " of "
							+ QString::number(thumbFileInfoList.size()) + " images");
	else if (nSelected == 1)
	{
		QString imageName = thumbViewModel->item(indexesList.first().row())->data(FileNameRole).toString();
		state = QString(imageName);

		imageInfoReader.setFileName(currentViewDir + QDir::separator() + imageName);
		if (imageInfoReader.size().isValid())
		{

			QFileInfo imageInfo = QFileInfo(currentViewDir + QDir::separator() + imageName);
			info =	"Image " + QString::number(indexesList.first().row() + 1) + " of " + QString::number(thumbFileInfoList.size())
					+ "      " + imageInfoReader.format().toUpper() + " File      "
					+ QString::number(imageInfoReader.size().height()) + "x"
					+ QString::number(imageInfoReader.size().width())
					+ "      " + QString::number(imageInfo.size() / 1024) + "K      "
					+  imageInfo.lastModified().toString(Qt::SystemLocaleShortDate);

		}
		else
		{
			imageInfoReader.read();
			info = imageInfoReader.errorString();
		}
	}

	emit updateState(state, info);
}

void ThumbView::startDrag(Qt::DropActions)
{
	QModelIndexList indexesList = selectionModel()->selectedIndexes();
	if (indexesList.isEmpty())
	{
		return;
	}

	QDrag *drag = new QDrag(this);
	QMimeData *mimeData = new QMimeData;
	drag->setMimeData(mimeData);
	drag->start(Qt::CopyAction | Qt::MoveAction);
}

void ThumbView::abort()
{
	abortOp = true;
}

void ThumbView::updateIndex()
{
	if (thumbLoaderActive)
		newIndex = getFirstVisibleItem();
}

int ThumbView::getFirstVisibleItem()
{
	QModelIndex idx;

	for (int currThumb = 0; currThumb < thumbViewModel->rowCount() - 1; ++currThumb)
	{
		idx = thumbViewModel->indexFromItem(thumbViewModel->item(currThumb));
		if (viewport()->rect().contains(QPoint(0, visualRect(idx).y() + visualRect(idx).height() + 1)))
		{
			return idx.row();
		}
	}

	return -1;
}

bool ThumbView::isItemVisible(QModelIndex idx)
{
	if (viewport()->rect().contains(QPoint(0, visualRect(idx).y() + visualRect(idx).height() + 1)))
	{
		return true;
	}

	return false;
}

void ThumbView::load(QString &cliImageName)
{
	float thumbAspect = 0;
	if (GData::thumbsLayout == Compact)
		thumbAspect = 1.77;
	else if (GData::thumbsLayout == Squares)
		thumbAspect = 1.5;

	thumbHeight = (GData::thumbsLayout == Squares)? thumbSize * thumbAspect : thumbSize;
	thumbWidth = (GData::thumbsLayout != Classic)? thumbHeight * thumbAspect : thumbHeight;
	setIconSize(QSize(thumbWidth, thumbHeight));

	thumbsDir->setPath(currentViewDir);
	thumbsDir->setSorting(thumbsSortFlags);
	
	abortOp = false;
	newIndex = 0;

	initThumbs();
	if (!cliImageName.isEmpty())
	{
		setCurrentIndexByName(cliImageName);
	}

	if ((thumbViewModel->rowCount()) == 1)
		emit updateState("No images", "");
	else 
	{
		QString state = (QString::number(thumbViewModel->rowCount() - 1) + " images");
		emit updateState(state, "");
	}

	loadThumbs();
}

void ThumbView::initThumbs()
{
	thumbFileInfoList = thumbsDir->entryInfoList();
	QStandardItem *thumbIitem;
	int currThumb;

	setSpacing(GData::thumbSpacing);
	thumbViewModel->clear();
	thumbIsLoaded->clear();

	if (isNeedScroll)
		scrollToTop();

	if (GData::thumbsLayout == Classic)
		setUniformItemSizes(true);
	else
		setUniformItemSizes(false);

	QPixmap emptyPixMap = QPixmap::fromImage(emptyImg).scaled(thumbWidth, thumbHeight);

	for (currThumb = 0; currThumb < thumbFileInfoList.size(); ++currThumb)
	{
		thumbFileInfo = thumbFileInfoList.at(currThumb);
		thumbIitem = new QStandardItem();
		thumbIitem->setData(currThumb, SortRole);
		thumbIitem->setData(thumbFileInfo.fileName(), FileNameRole);
		if (GData::thumbsLayout == Classic)
			thumbIitem->setData(thumbFileInfo.fileName(), Qt::DisplayRole);
		thumbIitem->setIcon(emptyPixMap);
		thumbIitem->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);

		thumbViewModel->appendRow(thumbIitem);
		thumbIsLoaded->append(false);

		if (GData::thumbsLayout == Squares)
		{
			int sqrSz = thumbWidth / 2;
			QSize hintSize(sqrSz, sqrSz);
			thumbViewModel->item(currThumb)->setSizeHint(hintSize);
		}
	}

	// Dummy image
	thumbIitem = new QStandardItem("");
	thumbIitem->setIcon(emptyPixMap);
	thumbViewModel->appendRow(thumbIitem);
	thumbIsLoaded->append(true);
	setRowHidden(thumbViewModel->rowCount() - 1, true);
}

void ThumbView::loadThumbs()
{
	QImageReader thumbReader;
	QSize currThumbSize;
	bool needRefresh = false;
	thumbLoaderActive = true;

	updateIndex();

refreshThumbs:
	for (int currThumb = 0; currThumb < thumbViewModel->rowCount() - 1; ++currThumb)
	{
		if (thumbIsLoaded->at(currThumb))
			continue;

		thumbFileInfo = thumbFileInfoList.at(currThumb);

		thumbReader.setFileName(thumbFileInfo.filePath());
		currThumbSize = thumbReader.size();

		if (currThumbSize.isValid())
		{
			if (!GData::noEnlargeSmallThumb || (currThumbSize.width() > thumbWidth || currThumbSize.height() > thumbHeight))
				currThumbSize.scale(QSize(thumbWidth, thumbHeight), Qt::KeepAspectRatio);
				
			thumbReader.setScaledSize(currThumbSize);
			thumbViewModel->item(currThumb)->setIcon(QPixmap::fromImage(thumbReader.read()));
		}
		else
		{
			thumbViewModel->item(currThumb)->setIcon(QIcon::fromTheme("image-missing",
													QIcon(":/images/error_image.png")).pixmap(64, 64));
		}

		if (GData::thumbsLayout == Compact)
		{
			if (isItemVisible(thumbViewModel->item(currThumb)->index()))
				setRowHidden(currThumb, false);
		}

		(*thumbIsLoaded)[currThumb] = true;

		QApplication::processEvents();

		if (newIndex)
		{
			currThumb = newIndex - 1;
			if (currThumb < 0)
				currThumb = 0;
			newIndex = 0;
			needRefresh = true;
			if (GData::thumbsLayout == Compact)
				setRowHidden(currThumb, false);
		}

		if (abortOp)
			break;
	}

	if (needRefresh)
	{
		needRefresh = false;
		goto refreshThumbs;
	}

	if (GData::thumbsLayout == Compact && thumbViewModel->rowCount() > 1)
	{
		setRowHidden(0 , false);
	}

	thumbLoaderActive = false;
	emit unsetBusy();
}

void ThumbView::wheelEvent(QWheelEvent *event)
{
	if (event->delta() < 0)
		verticalScrollBar()->setValue(verticalScrollBar()->value() + thumbHeight);
	else
		verticalScrollBar()->setValue(verticalScrollBar()->value() - thumbHeight);
}

void ThumbView::invertSelection()
{
	QModelIndex idx;

	for (int currThumb = 0; currThumb < thumbViewModel->rowCount() - 1; ++currThumb)
	{
		idx = thumbViewModel->indexFromItem(thumbViewModel->item(currThumb));
		selectionModel()->select(idx, QItemSelectionModel::Toggle);
	}
}

FSTree::FSTree(QWidget *parent) : QTreeView(parent)
{
	setAcceptDrops(true);
	setDragEnabled(true);
	setDragDropMode(QAbstractItemView::InternalMove);

	connect(this, SIGNAL(expanded(const QModelIndex &)),
								this, SLOT(resizeTreeColumn(const QModelIndex &)));
	connect(this, SIGNAL(collapsed(const QModelIndex &)),
								this, SLOT(resizeTreeColumn(const QModelIndex &)));
}

QModelIndex FSTree::getCurrentIndex()
{
	return selectedIndexes().first();
}

void FSTree::resizeTreeColumn(const QModelIndex &)
{
	resizeColumnToContents(0);
}

void FSTree::dragEnterEvent(QDragEnterEvent *event)
{
	QModelIndexList selectedDirs = selectionModel()->selectedRows();
	dndOrigSelection = selectedDirs[0];
	event->acceptProposedAction();
}

void FSTree::dragMoveEvent(QDragMoveEvent *event)
{
	setCurrentIndex(indexAt(event->pos()));
}

void FSTree::dropEvent(QDropEvent *event)
{
	if (event->source())
	{
		QString fstreeStr="FSTree";
		bool dirOp = (event->source()->metaObject()->className() == fstreeStr);
		emit dropOp(event->keyboardModifiers(), dirOp, event->mimeData()->urls().at(0).toLocalFile());
		setCurrentIndex(dndOrigSelection);
	}
}

