/****************************************************************
 * This file is distributed under the following license:
 *
 * Copyright (C) 2014 RetroShare Team
 * Copyright (c) 2006-2007, crypton
 * Copyright (c) 2006, Matt Edman, Justin Hipple
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

#ifndef WINDOWS_SYS
#include <sys/times.h>
#endif

#include <iostream>
#include <QtGlobal>
#include <QPainter>
#include <QDateTime>
#include <QTimer>

#include "RSGraphWidget.h"

RSGraphSource::RSGraphSource()
{
    _time_limit_msecs    = 10*1000 ;
    _update_period_msecs =  1*1000 ;
    _time_orig_msecs     = QDateTime::currentMSecsSinceEpoch() ;
    _timer = new QTimer ;
    QObject::connect(_timer,SIGNAL(timeout()),this,SLOT(update())) ;
}

RSGraphSource::~RSGraphSource()
{
    stop() ;
    delete _timer ;
}
void RSGraphSource::clear()
{
    _points.clear() ;
}
void RSGraphSource::stop()
{
    _timer->stop() ;
}
void RSGraphSource::start()
{
    _timer->stop();
    _timer->start((int)(_update_period_msecs)) ;
}

void RSGraphSource::getDataPoints(int index,std::vector<QPointF>& pts) const
{
    pts.clear() ;
    qint64 now = getTime() ;

    for(std::list<std::pair<qint64,std::vector<float> > >::const_iterator it = _points.begin();it!=_points.end();++it)
        pts.push_back(QPointF( (now - (*it).first)/1000.0f,(*it).second[index])) ;
}

void RSGraphWidget::addSource(RSGraphSource *gs)
{
    _sources.push_back(gs) ;
}

qint64 RSGraphSource::getTime() const
{
    return QDateTime::currentMSecsSinceEpoch() - _time_orig_msecs ;
}

void RSGraphSource::update()
{
    std::vector<float> vals ;
    getValues(vals) ;

    qint64 ms = getTime() ;

    _points.push_back(std::make_pair(ms,vals)) ;

    //std::cerr << "RSGraphSource::update() Got new data for time " << ms/1000.0 << ", now=" << ms << std::endl;

    // eliminate measurements that stand beyond the time limit

    for(std::list<std::pair<qint64,std::vector<float> > >::iterator it=_points.begin();it!=_points.end();)
        if( ms - (*it).first > _time_limit_msecs)
        {
            //std::cerr << "  removing old value with time " << (*it).first/1000.0f << std::endl;
            it = _points.erase(it) ;
        }
        else
            break ;
}

void RSGraphSource::setCollectionTimeLimit(qint64 s) { _time_limit_msecs = s ; }
void RSGraphSource::setCollectionTimePeriod(qint64 s) { _update_period_msecs = s ; }

void RSGraphWidget::setTimeScale(float pixels_per_second)
{
    _time_scale =pixels_per_second ;
}

/** Default contructor */
RSGraphWidget::RSGraphWidget(QWidget *parent)
: QFrame(parent)
{
  _painter = new QPainter();
  _graphStyle = AreaGraph;
  
  /* Initialize graph values */
  _maxPoints = getNumPoints();  
  _maxValue = MINUSER_SCALE;

  _time_scale = 5.0f ; // in pixels per second.
  _timer = new QTimer ;
  QObject::connect(_timer,SIGNAL(timeout()),this,SLOT(update())) ;

  _timer->start(1000);
}

/** Default destructor */
RSGraphWidget::~RSGraphWidget()
{
  delete _painter;

    for(uint32_t i=0;i<_sources.size();++i)
        delete _sources[i] ;
}

/** Gets the width of the desktop, which is the maximum number of points 
 * we can plot in the graph. */
int
RSGraphWidget::getNumPoints()
{
  QDesktopWidget *desktop = QApplication::desktop();
  int width = desktop->width();
  return width;
}

/** Clears the graph. */
void
RSGraphWidget::resetGraph()
{
  _maxValue = MINUSER_SCALE;
  update();
}

/** Toggles display of respective graph lines and counters. */
//void
//DhtGraph::setShowCounters(bool showRSDHT, bool showALLDHT)
//{
//  _showRSDHT = showRSDHT;
//  _showALLDHT = showALLDHT;
//  this->update();
//}

/** Overloads default QWidget::paintEvent. Draws the actual 
 * bandwidth graph. */
void RSGraphWidget::paintEvent(QPaintEvent *)
{
    //std::cerr << "In paint event!" << std::endl;

  /* Set current graph dimensions */
  _rec = this->frameRect();
  
  /* Start the painter */
  _painter->begin(this);
  
  /* We want antialiased lines and text */
  _painter->setRenderHint(QPainter::Antialiasing);
  _painter->setRenderHint(QPainter::TextAntialiasing);
  
  /* Fill in the background */
  _painter->fillRect(_rec, QBrush(BACK_COLOR));
  _painter->drawRect(_rec);

  /* Paint the scale */
  paintScale();
  /* Plot the rsDHT/allDHT data */
  paintData();
  /* Paint the rsDHT/allDHT totals */
  paintTotals();

  /* Stop the painter */
  _painter->end();
}

/** Paints an integral and an outline of that integral for each data set (rsdht
 * and/or alldht) that is to be displayed. The integrals will be drawn first,
 * followed by the outlines, since we want the area of overlapping integrals
 * to blend, but not the outlines of those integrals. */
void RSGraphWidget::paintData()
{
    /* Convert the bandwidth data points to graph points */
    QVector<QPointF> points ;
    pointsFromData(points) ;

    /* Plot the bandwidth data as area graphs */
    if (_graphStyle == AreaGraph)
        paintIntegral(points, RSDHT_COLOR, 0.6);

    /* Plot the bandwidth as solid lines. If the graph style is currently an
   * area graph, we end up outlining the integrals. */
        paintLine(points, RSDHT_COLOR);
}

/** Returns a list of points on the bandwidth graph based on the supplied set
 * of rsdht or alldht values. */
void RSGraphWidget::pointsFromData(QVector<QPointF>& points)
{
    points.clear();

  int x = _rec.width();
  int y = _rec.height();
  qreal scale = (y - (y/10)) / _maxValue;
  qreal currValue;

  if(_sources.empty())
      return ;

  const RSGraphSource& source(*_sources.front()) ;

  float time_step = 1.0f ;	// number of seconds per pixel

  /* Translate all data points to points on the graph frame */

  std::vector<QPointF> values ;

  source.getDataPoints(0,values) ;
  float last = values.back().x() ;

  //std::cerr << "Got " << values.size() << " values for index 0" << std::endl;

  float last_px = SCALE_WIDTH ;
  float last_py = 0.0f ;

  for (int i = 0; i < values.size(); ++i)
  {
    //std::cerr << "Value: (" << values[i].x() << " , " << values[i].y() << ")" << std::endl;

    // compute point in pixels

    float px = x - (values[i].x()-last)*_time_scale ;
    float py = y - values[i].y()*scale ;

    if(px >= SCALE_WIDTH && last_px < SCALE_WIDTH)
    {
        float alpha = (SCALE_WIDTH - last_px)/(px - last_px) ;
        float ipx = SCALE_WIDTH ;
        float ipy = (1-alpha)*last_py + alpha*py ;

        points << QPointF(ipx,y) ;
        points << QPointF(ipx,ipy) ;
    }
    else if(i==0)
        points << QPointF(px,y) ;

    last_px = px ;
    last_py = py ;

    if(px < SCALE_WIDTH)
        continue ;

    // remove midle point when 3 consecutive points have the same value.

    if(points.size() > 1 && points[points.size()-2].y() == points.back().y() && points.back().y() == py)
        points.pop_back() ;

    points << QPointF(px,py) ;

    if(i==values.size()-1)
        points << QPointF(px,y) ;
  }
}

/** Plots an integral using the data points in <b>points</b>. The area will be
 * filled in using <b>color</b> and an alpha-blending level of <b>alpha</b>
 * (default is opaque). */
void RSGraphWidget::paintIntegral(const QVector<QPointF>& points, QColor color, qreal alpha)
{
  /* Save the current brush, plot the integral, and restore the old brush */
  QBrush oldBrush = _painter->brush();
  color.setAlphaF(alpha);
  _painter->setBrush(QBrush(color));
  _painter->drawPolygon(points.data(), points.size());
  _painter->setBrush(oldBrush);
}

/** Iterates the input list and draws a line on the graph in the appropriate
 * color. */
void RSGraphWidget::paintLine(const QVector<QPointF>& points, QColor color, Qt::PenStyle lineStyle)
{
  /* Save the current brush, plot the line, and restore the old brush */
  QPen oldPen = _painter->pen();
  _painter->setPen(QPen(color, lineStyle));
  _painter->drawPolyline(points.data(), points.size());
  _painter->setPen(oldPen);
}

/** Paints selected total indicators on the graph. */
void RSGraphWidget::paintTotals()
{
  int x = SCALE_WIDTH + FONT_SIZE, y = 0;
  int rowHeight = FONT_SIZE;

#if !defined(Q_WS_MAC)
  /* On Mac, we don't need vertical spacing between the text rows. */
  rowHeight += 5;
#endif

//  /* If total received is selected */
//    y = rowHeight;
//    _painter->setPen(RSDHT_COLOR);
//    _painter->drawText(x, y,
//        tr("RetroShare users in DHT: ")+
//        " ("+tr("%1").arg(_rsDHT->first(), 0, 'f', 0)+")");
}

/** Returns a formatted string with the correct size suffix. */
QString RSGraphWidget::totalToStr(qreal total)
{
  /* Determine the correct size suffix */
  if (total < 1024) {
    /* Use KB suffix */
    return tr("%1 KB").arg(total, 0, 'f', 2);
  } else if (total < 1048576) {
    /* Use MB suffix */
    return tr("%1 MB").arg(total/1024.0, 0, 'f', 2);
  } else {
    /* Use GB suffix */
    return tr("%1 GB").arg(total/1048576.0, 0, 'f', 2);
  }
}

/** Paints the scale on the graph. */
void RSGraphWidget::paintScale()
{
  qreal markStep = _maxValue * .25;
  int top = _rec.y();
  int bottom = _rec.height();
  qreal paintStep = (bottom - (bottom/10)) / 4;
  
  /* Draw the other marks in their correctly scaled locations */
  qreal scale;
  qreal pos;
  for (int i = 1; i < 5; i++) {
    pos = bottom - (i * paintStep);
    scale = i * markStep;
    _painter->setPen(SCALE_COLOR);
    _painter->drawText(QPointF(5, pos+FONT_SIZE), 
                       tr("%1 Users").arg(scale, 0, 'f', 0));
    _painter->setPen(GRID_COLOR);
    _painter->drawLine(QPointF(SCALE_WIDTH, pos), 
                       QPointF(_rec.width(), pos));
  }
  
  /* Draw vertical separator */
  _painter->drawLine(SCALE_WIDTH, top, SCALE_WIDTH, bottom);
}

