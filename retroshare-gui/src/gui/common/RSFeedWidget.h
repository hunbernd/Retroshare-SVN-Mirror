/****************************************************************
 * This file is distributed under the following license:
 *
 * Copyright (c) 2014, RetroShare Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/

#ifndef _RSFEEDTREEWIDGET_H
#define _RSFEEDTREEWIDGET_H

#include <QAbstractItemView>
#include <QWidget>
#include <QMap>

#include "retroshare/rsgxsifacetypes.h"

#define FEED_TREEWIDGET_SORTROLE Qt::UserRole

class FeedItem;
class QTreeWidgetItem;
class RSTreeWidgetItemCompareRole;
class GxsFeedItem;

namespace Ui {
class RSFeedWidget;
}

typedef void (*RSFeedWidgetCallbackFunction)(FeedItem *feedItem, const QVariant &data);
typedef bool (*RSFeedWidgetFindCallbackFunction)(FeedItem *feedItem, const QVariant &data1, const QVariant &data2);
typedef bool (*RSFeedWidgetFilterCallbackFunction)(FeedItem *feedItem, const QString &text, int filter);

class RSFeedWidget : public QWidget
{
	Q_OBJECT

public:
	RSFeedWidget(QWidget *parent = 0);
	virtual ~RSFeedWidget();

	void addFeedItem(FeedItem *feedItem, Qt::ItemDataRole sortRole, const QVariant &value);
	void addFeedItem(FeedItem *feedItem, const QMap<Qt::ItemDataRole, QVariant> &sort);

	void setSort(FeedItem *feedItem, Qt::ItemDataRole sortRole, const QVariant &value);
	void setSort(FeedItem *feedItem, const QMap<Qt::ItemDataRole, QVariant> &sort);

	void removeFeedItem(FeedItem *feedItem);
	void clear();

	void setSortRole(Qt::ItemDataRole role, Qt::SortOrder order);
	void setSortingEnabled(bool enable);
	void setFilterCallback(RSFeedWidgetFilterCallbackFunction callback);

	void enableRemove(bool enable);
	void setSelectionMode(QAbstractItemView::SelectionMode mode);

	bool scrollTo(FeedItem *feedItem, bool focus);

	void withAll(RSFeedWidgetCallbackFunction callback, const QVariant &data);
	FeedItem *findFeedItem(RSFeedWidgetFindCallbackFunction callback, const QVariant &data1, const QVariant &data2);

	void selectedFeedItems(QList<FeedItem*> &feedItems);

	/* Convenience functions */
	GxsFeedItem *findGxsFeedItem(const RsGxsGroupId &groupId, const RsGxsMessageId &messageId);

public slots:
	void setFilter(const QString &text, int type);
	void setFilterText(const QString &text);
	void setFilterType(int type);

protected:
	bool eventFilter(QObject *object, QEvent *event);

private slots:
	void feedItemDestroyed(FeedItem *feedItem);
	void feedItemSizeChanged(FeedItem *feedItem);

private:
	void connectSignals(FeedItem *feedItem);
	void disconnectSignals(FeedItem *feedItem);
	FeedItem *feedItemFromTreeItem(QTreeWidgetItem *treeItem);
	QTreeWidgetItem *findTreeWidgetItem(FeedItem *feedItem);
	void filterItems();
	void filterItem(QTreeWidgetItem *treeItem, FeedItem *feedItem);

private:
	/* Sort */
	RSTreeWidgetItemCompareRole *mFeedCompareRole;

	/* Filter */
	RSFeedWidgetFilterCallbackFunction mFilterCallback;
	QString mFilterText;
	int mFilterType;

	/* Remove */
	bool mEnableRemove;

	Ui::RSFeedWidget *ui;
};

#endif
