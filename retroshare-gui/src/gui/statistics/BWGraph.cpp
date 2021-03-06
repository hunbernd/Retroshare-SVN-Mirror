#include "BWGraph.h"

#include <time.h>

//#define BWGRAPH_DEBUG 1

void BWGraphSource::update()
{
#ifdef BWGRAPH_DEBUG
    std::cerr << "Updating BW graphsource..." << std::endl;
#endif

    TrafficHistoryChunk thc ;
    rsConfig->getTrafficInfo(thc.out_rstcl,thc.in_rstcl);

#ifdef BWGRAPH_DEBUG
    std::cerr << "  got " << std::dec << thc.out_rstcl.size() << " out clues" << std::endl;
    std::cerr << "  got " << std::dec << thc.in_rstcl.size() << " in clues" << std::endl;
#endif

    // keep track of them, in case we need to change the sorting

    thc.time_stamp = getTime() ;
    mTrafficHistory.push_back(thc) ;

    // add visible friends/services

    for(std::list<RSTrafficClue>::const_iterator it(thc.out_rstcl.begin());it!=thc.out_rstcl.end();++it)
    {
        mVisibleFriends.insert(it->peer_id) ;
        mVisibleServices.insert(it->service_id) ;
    }

    for(std::list<RSTrafficClue>::const_iterator it(thc.in_rstcl.begin());it!=thc.in_rstcl.end();++it)
    {
        mVisibleFriends.insert(it->peer_id) ;
        mVisibleServices.insert(it->service_id) ;
    }

#ifdef BWGRAPH_DEBUG
    std::cerr << "  visible friends: " << std::dec << mVisibleFriends.size() << std::endl;
    std::cerr << "  visible service: " << std::dec << mVisibleServices.size() << std::endl;
#endif

    // now, convert data to current curve points.

    std::map<std::string,float> vals ;
    convertTrafficClueToValues(thc.out_rstcl,vals) ;

    qint64 ms = getTime() ;

    for(std::map<std::string,float>::iterator it=vals.begin();it!=vals.end();++it)
    {
        std::list<std::pair<qint64,float> >& lst(_points[it->first]) ;

        lst.push_back(std::make_pair(ms,it->second)) ;

        for(std::list<std::pair<qint64,float> >::iterator it2=lst.begin();it2!=lst.end();)
            if( ms - (*it2).first > _time_limit_msecs)
            {
                //std::cerr << "  removing old value with time " << (*it).first/1000.0f << std::endl;
                it2 = lst.erase(it2) ;
            }
            else
                break ;
    }

    // remove empty lists

    for(std::map<std::string,std::list<std::pair<qint64,float> > >::iterator it=_points.begin();it!=_points.end();)
        if(it->second.empty())
    {
        std::map<std::string,std::list<std::pair<qint64,float> > >::iterator tmp(it) ;
        ++tmp;
        _points.erase(it) ;
        it=tmp ;
    }
        else
            ++it ;

    // also clears history

    for(std::list<TrafficHistoryChunk>::iterator it = mTrafficHistory.begin();it!=mTrafficHistory.end();++it)
    {
#ifdef BWGRAPH_DEBUG
        std::cerr << "TS=" << (*it).time_stamp << ", ms = " << ms << ", diff=" << ms - (*it).time_stamp  << " compared to  " << _time_limit_msecs << std::endl;
#endif

            if( ms - (*it).time_stamp > _time_limit_msecs)
            {
                it = mTrafficHistory.erase(it) ;

#ifdef BWGRAPH_DEBUG
                std::cerr << "Removing 1 item of traffic history" << std::endl;
#endif
            }
            else
                break ;
    }

#ifdef BWGRAPH_DEBUG
    std::cerr << "Traffic history has size " << mTrafficHistory.size() << std::endl;
#endif
}

void BWGraphSource::convertTrafficClueToValues(const std::list<RSTrafficClue>& lst,std::map<std::string,float>& vals) const
{
	vals.clear() ;

	switch(_friend_graph_type)
	{
	case GRAPH_TYPE_SINGLE:
		switch(_service_graph_type)
		{
		case GRAPH_TYPE_SINGLE:		// single friend, single service => one curve per service sub_id
		{
			std::vector<RSTrafficClue> clue_per_sub_id(256) ;

			for(std::list<RSTrafficClue>::const_iterator  it(lst.begin());it!=lst.end();++it)
				if(it->peer_id == _current_selected_friend && it->service_id == _current_selected_service)
					clue_per_sub_id[it->service_sub_id] += *it ;

			for(uint32_t i=0;i<256;++i)
				if(clue_per_sub_id[i].count > 0)
					vals[QString::number(i,16).toStdString()] = (_current_unit == UNIT_KILOBYTES)?(clue_per_sub_id[i].size/1000.0f):(clue_per_sub_id[i].count) ;
		}
			break ;

		case GRAPH_TYPE_ALL:		// single friend, all services => one curve per service id
		{
			std::map<uint16_t,RSTrafficClue> clue_per_id ;

			for(std::list<RSTrafficClue>::const_iterator  it(lst.begin());it!=lst.end();++it)
				if(it->peer_id == _current_selected_friend)
					clue_per_id[it->service_id] += *it ;

			for(std::map<uint16_t,RSTrafficClue>::const_iterator it(clue_per_id.begin());it!=clue_per_id.end();++it)
				vals[QString::number(it->first,16).toStdString()] = (_current_unit == UNIT_KILOBYTES)?(it->second.size/1000.0f):(it->second.count) ;
		}
			break ;
		case GRAPH_TYPE_SUM:	// single friend, sum services => one curve
		{
			RSTrafficClue total ;
			std::map<uint16_t,RSTrafficClue> clue_per_id ;

			for(std::list<RSTrafficClue>::const_iterator  it(lst.begin());it!=lst.end();++it)
				if(it->peer_id == _current_selected_friend)
					total += *it ;

			vals[_current_selected_friend_name] = (_current_unit == UNIT_KILOBYTES)?(total.size/1000.0f):(total.count) ;
		}
		}
		break ;

	case GRAPH_TYPE_ALL:
		switch(_service_graph_type)
		{
		case GRAPH_TYPE_SINGLE: // all friends, single service => one curve per friend for that service
		{
			std::map<RsPeerId,RSTrafficClue> clue_per_peer_id;

			for(std::list<RSTrafficClue>::const_iterator  it(lst.begin());it!=lst.end();++it)
				if(it->service_id == _current_selected_service)
					clue_per_peer_id[it->peer_id] += *it ;

			for(std::map<RsPeerId,RSTrafficClue>::const_iterator it(clue_per_peer_id.begin());it!=clue_per_peer_id.end();++it)
				vals[it->first.toStdString()] = (_current_unit == UNIT_KILOBYTES)?(it->second.size/1000.0f):(it->second.count) ;
		}
			break ;

		case GRAPH_TYPE_ALL: std::cerr << "(WW) Impossible situation. Cannot draw graph in mode All/All. Reverting to sum." << std::endl;
		case GRAPH_TYPE_SUM:		// all friends, sum of services => one curve per friend
		{
			RSTrafficClue total ;
			std::map<RsPeerId,RSTrafficClue> clue_per_peer_id ;

			for(std::list<RSTrafficClue>::const_iterator  it(lst.begin());it!=lst.end();++it)
				clue_per_peer_id[it->peer_id] += *it;

			for(std::map<RsPeerId,RSTrafficClue>::const_iterator it(clue_per_peer_id.begin());it!=clue_per_peer_id.end();++it)
				vals[it->first.toStdString()] = (_current_unit == UNIT_KILOBYTES)?(it->second.size/1000.0f):(it->second.count) ;
		}
			break ;
		}
		break ;

	case GRAPH_TYPE_SUM:
		switch(_service_graph_type)
		{
		case GRAPH_TYPE_SINGLE:	// sum of friends, single service => one curve per service sub id summed over all friends
		{
			std::vector<RSTrafficClue> clue_per_sub_id(256) ;

			for(std::list<RSTrafficClue>::const_iterator  it(lst.begin());it!=lst.end();++it)
				if(it->service_id == _current_selected_service)
					clue_per_sub_id[it->service_sub_id] += *it ;

			for(uint32_t i=0;i<256;++i)
				if(clue_per_sub_id[i].count > 0)
					vals[QString::number(i,16).toStdString()] = (_current_unit == UNIT_KILOBYTES)?(clue_per_sub_id[i].size/1000.0f):(clue_per_sub_id[i].count) ;
		}
			break ;

		case GRAPH_TYPE_ALL:	// sum of friends, all services => one curve per service id summed over all friends
		{
			std::map<uint16_t,RSTrafficClue> clue_per_service ;

			for(std::list<RSTrafficClue>::const_iterator  it(lst.begin());it!=lst.end();++it)
				clue_per_service[it->service_id] += *it;

			for(std::map<uint16_t,RSTrafficClue>::const_iterator it(clue_per_service.begin());it!=clue_per_service.end();++it)
				vals[QString::number(it->first,16).toStdString()] = (_current_unit == UNIT_KILOBYTES)?(it->second.size/1000.0f):(it->second.count) ;
		}
			break ;

		case GRAPH_TYPE_SUM: 	// sum of friends, sum of services => one single curve covering the total bandwidth
		{
			RSTrafficClue total ;

			for(std::list<RSTrafficClue>::const_iterator  it(lst.begin());it!=lst.end();++it)
				total += *it;

			vals[QString("Total").toStdString()] = (_current_unit == UNIT_KILOBYTES)?(total.size/1000.0f):(total.count) ;
		}
			break ;
		}
		break ;
	}
}

BWGraphSource::BWGraphSource()
	: RSGraphSource()
{

    _total_sent =0;
    _total_recv =0;

    _friend_graph_type = GRAPH_TYPE_SUM;
    _service_graph_type = GRAPH_TYPE_SUM;

    _current_selected_friend.clear() ;
    _current_selected_service = 0;
    _current_unit = UNIT_KILOBYTES;
    _current_direction = DIRECTION_UP;

}

void BWGraphSource::getValues(std::map<std::string,float>& values) const
{
    RsConfigDataRates totalRates;
    rsConfig->getTotalBandwidthRates(totalRates);

    values.insert(std::make_pair(std::string("Bytes in"),1024 * (float)totalRates.mRateIn)) ;
    values.insert(std::make_pair(std::string("Bytes out"),1024 * (float)totalRates.mRateOut)) ;

    _total_sent += 1024 * totalRates.mRateOut * _update_period_msecs/1000.0f ;
    _total_recv += 1024 * totalRates.mRateIn * _update_period_msecs/1000.0f ;
}

QString BWGraphSource::unitName() const { return (_current_unit == UNIT_KILOBYTES)?tr("KB/s"):QString(); }

QString BWGraphSource::displayValue(float v) const
{
    if(_current_unit == UNIT_KILOBYTES)
    {
	    if(v < 1000)
		    return QString::number(v,'f',2) + " B/s" ;
	    else if(v < 1000*1024)
		    return QString::number(v/1024.0,'f',2) + " KB/s" ;
	    else
		    return QString::number(v/(1024.0*1024),'f',2) + " MB/s" ;
    }
    else if(_current_unit == UNIT_COUNT)
    {
	    if(v < 1000)
		    return QString::number(v,'f',0) ;
	    else if(v < 1000*1024)
		    return QString::number(v/1024.0,'f',1) + "k" ;
	    else
		    return QString::number(v/(1024.0*1024),'f',1) + "M" ;
    }
    else
    return QString() ;
}

QString BWGraphSource::legend(int i,float v) const
{
    if(i==0)
        return RSGraphSource::legend(i,v) + " Total: " + niceNumber(_total_recv) ;
    else
        return RSGraphSource::legend(i,v) + " Total: " + niceNumber(_total_sent) ;
}
QString BWGraphSource::niceNumber(float v) const
{
    if(v < 1000)
        return QString::number(v,'f',2) + " B" ;
    else if(v < 1000*1024)
        return QString::number(v/1024.0,'f',2) + " KB" ;
    else if(v < 1000*1024*1024)
        return QString::number(v/(1024*1024.0),'f',2) + " MB" ;
    else
        return QString::number(v/(1024*1024.0*1024),'f',2) + " GB";
}

void BWGraphSource::setSelector(int selector_type,int graph_type,const std::string& selector_client_string)
{
    std::cerr << "Setting Graph Source selector to " << selector_type << " - " << graph_type << " - " << selector_client_string << std::endl;

    bool changed = false ;

    if(selector_type == SELECTOR_TYPE_FRIEND && (_friend_graph_type != graph_type || (graph_type == GRAPH_TYPE_SINGLE && selector_client_string != _current_selected_friend.toStdString())))
    {
	    if(graph_type ==  GRAPH_TYPE_SINGLE)
	    {

		    RsPeerId ns(selector_client_string) ;

		    if(!ns.isNull())
		    {
			    _current_selected_friend = ns ;
			    changed = true ;
			    _friend_graph_type = graph_type ;
		    }

		    else
			    std::cerr << "(EE) Cannot set current friend to " << selector_client_string << ": unrecognized friend string." << std::endl;
	    }
	    else
	    {
		    changed = true ;
		    _friend_graph_type = graph_type ;
	    }
    }
    else if(selector_type == SELECTOR_TYPE_SERVICE
                    && (_service_graph_type != graph_type || (graph_type == GRAPH_TYPE_SINGLE && selector_client_string != QString::number(_current_selected_service,16).toStdString())))
    {
	    if(graph_type ==  GRAPH_TYPE_SINGLE)
	    {
		    bool ok = false ;
		    int tmp = QString::fromStdString(selector_client_string).toInt() ;

		    if(tmp > 0 && tmp < 0x10000)
		    {
			    _current_selected_service = tmp ;

			    changed = true ;
			    _service_graph_type = graph_type ;
		    }
                    else
			    std::cerr << "(EE) Cannot set current service to " << selector_client_string << ": unrecognized service string." << std::endl;

	    }
	    else
	    {
		    changed = true ;
		    _service_graph_type = graph_type ;
	    }
    }

    // now re-convert all traffic history into the appropriate curves

    if(changed)
	    recomputeCurrentCurves() ;
}
void BWGraphSource::setUnit(int unit)
{
    if(unit == _current_unit)
        return ;

    _current_unit = unit ;

    recomputeCurrentCurves() ;
}
void BWGraphSource::setDirection(int dir)
{
    if(dir == _current_direction)
        return ;

    _current_direction = dir ;

    recomputeCurrentCurves() ;
}
void BWGraphSource::recomputeCurrentCurves()
{
    std::cerr << "BWGraphSource: recomputing current curves." << std::endl;

    _points.clear() ;

    // now, convert data to current curve points.

    for(std::list<TrafficHistoryChunk>::const_iterator it(mTrafficHistory.begin());it!=mTrafficHistory.end();++it)
    {
	    std::map<std::string,float> vals ;
	    qint64 ms = (*it).time_stamp ;

	    if(_current_direction==DIRECTION_UP)
		    convertTrafficClueToValues((*it).out_rstcl,vals) ;
	    else
		    convertTrafficClueToValues((*it).in_rstcl,vals) ;

	    for(std::map<std::string,float>::iterator it2=vals.begin();it2!=vals.end();++it2)
		    _points[it2->first].push_back(std::make_pair(ms,it2->second)) ;
    }

    std::cerr << "  points() contains " << _points.size() << " curves." << std::endl;
}

BWGraph::BWGraph(QWidget *parent) : RSGraphWidget(parent)
{
    _local_source = new BWGraphSource() ;

    std::cerr << "creaitng new BWGraph Source " << (void*)_local_source << std::endl;
    _local_source->setCollectionTimeLimit(30*60*1000) ; // 30  mins
    _local_source->setCollectionTimePeriod(1000) ;      // collect every second
    _local_source->setDigits(2) ;
    _local_source->start() ;
    _local_source->setUnit(BWGraphSource::UNIT_KILOBYTES) ;
    _local_source->setDirection(BWGraphSource::DIRECTION_UP) ;
    _local_source->setSelector(BWGraphSource::SELECTOR_TYPE_FRIEND,BWGraphSource::GRAPH_TYPE_ALL) ;
    _local_source->setSelector(BWGraphSource::SELECTOR_TYPE_SERVICE,BWGraphSource::GRAPH_TYPE_SUM) ;

    setSource(_local_source) ;

    setTimeScale(1.0f) ; // 1 pixels per second of time.

    setFlags(RSGRAPH_FLAGS_LOG_SCALE_Y) ;
    resetFlags(RSGRAPH_FLAGS_PAINT_STYLE_PLAIN) ;

    setFlags(RSGRAPH_FLAGS_SHOW_LEGEND) ;
}

BWGraphSource *BWGraph::source()
{
    return _local_source ;
}
