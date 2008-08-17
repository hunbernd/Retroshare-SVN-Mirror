/*
 * libretroshare/src/ft: pqitestor.cc
 *
 * File Transfer for RetroShare.
 *
 * Copyright 2008 by Robert Fernie.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License Version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 *
 * Please report all bugs and problems to "retroshare@lunamutt.com".
 *
 */

#include "ft/pqitestor.h"
#include "pqi/p3connmgr.h"


P3Hub::P3Hub()
{
	return;
}

void	P3Hub::addP3Pipe(std::string id, P3Pipe *pqi, p3ConnectMgr *mgr)
{
	hubItem item(id, pqi, mgr);

	std::map<std::string, hubItem>::iterator it;
	for(it = mPeers.begin(); it != mPeers.end(); it++)
	{
		(it->second).mConnMgr->connectResult(id, true, 0);
		mgr->connectResult(it->first, true, 0);
	}

	mPeers[id] = item;

	/* tell all the other peers we are connected */
	std::cerr << "P3Hub::addPQIPipe()";
	std::cerr << std::endl;

}


void 	P3Hub::run()
{
	RsItem *item;
	std::list<RsItem *> recvdQ;
	std::list<RsItem *>::iterator lit;
	while(1)
	{
		std::cerr << "P3Hub::run()";
		std::cerr << std::endl;

		std::map<std::string, hubItem>::iterator it;
		for(it = mPeers.begin(); it != mPeers.end(); it++)
		{
			while (NULL != (item = it->second.mPQI->PopSentItem()))
			{
				std::cerr << "P3Hub::run() recvd msg from: ";
				std::cerr << it->first;
				std::cerr << std::endl;
				item->print(std::cerr, 10);
				std::cerr << std::endl;

				recvdQ.push_back(item);
			}
		}

		/* now send out */
		for(lit = recvdQ.begin(); lit != recvdQ.end(); lit++)
		{
			std::string pId = (*lit)->PeerId();
			if (mPeers.end() == (it = mPeers.find(pId)))
			{
				std::cerr << "Failed to Find destination: " << pId;
				std::cerr << std::endl;
			}
			std::cerr << "P3Hub::run() sending msg to: ";
			std::cerr << it->first;
			std::cerr << std::endl;
			(*lit)->print(std::cerr, 10);
			std::cerr << std::endl;

			(it->second).mPQI->PushRecvdItem(*lit);
		}

		recvdQ.clear();



		/* Tick the Connection Managers (normally done by rsserver)
		 */

		/* sleep a bit */
		sleep(1);
	}
}

		
		
	


PQIPipe::PQIPipe(std::string peerId)
	:PQInterface(peerId)
{
	return;
}

int PQIPipe::SendItem(RsItem *item)
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	mSentItems.push_back(item);

	return 1;
}

RsItem *PQIPipe::PopSentItem()
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	if (mSentItems.size() == 0)
	{
		return NULL;
	}

	RsItem *item = mSentItems.front();
	mSentItems.pop_front();
	
	return item;
}

int PQIPipe::PushRecvdItem(RsItem *item)
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	mRecvdItems.push_back(item);

	return 1;
}

RsItem *PQIPipe::GetItem()
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	if (mRecvdItems.size() == 0)
	{
		return NULL;
	}

	RsItem *item = mRecvdItems.front();
	mRecvdItems.pop_front();
	
	return item;
}




/***** P3Pipe here *****/




int P3Pipe::SendAllItem(RsItem *item)
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	mSentItems.push_back(item);

	return 1;
}

RsItem *P3Pipe::PopSentItem()
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	if (mSentItems.size() == 0)
	{
		return NULL;
	}

	RsItem *item = mSentItems.front();
	mSentItems.pop_front();
	
	return item;
}

int P3Pipe::PushRecvdItem(RsItem *item)
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	RsCacheRequest *rcr;
	RsCacheItem *rci;
	RsFileRequest *rfr;
	RsFileData *rfd;
	RsRawItem *rri;

	if (NULL != (rcr = dynamic_cast<RsCacheRequest *>(item)))
	{
		mRecvdRsCacheRequests.push_back(rcr);
	}
	else if (NULL != (rci = dynamic_cast<RsCacheItem *>(item)))
	{
		mRecvdRsCacheItems.push_back(rci);
	}
	else if (NULL != (rfr = dynamic_cast<RsFileRequest *>(item)))
	{
		mRecvdRsFileRequests.push_back(rfr);
	}
	else if (NULL != (rfd = dynamic_cast<RsFileData *>(item)))
	{
		mRecvdRsFileDatas.push_back(rfd);
	}
	else if (NULL != (rri = dynamic_cast<RsRawItem *>(item)))
	{
		mRecvdRsRawItems.push_back(rri);
	}

	return 1;
}

int	P3Pipe::SearchSpecific(RsCacheRequest *item)
{
	SendAllItem(item);
	return 1;
}

int     P3Pipe::SendSearchResult(RsCacheItem *item)
{
	SendAllItem(item);
	return 1;
}

int     P3Pipe::SendFileRequest(RsFileRequest *item)
{
	SendAllItem(item);
	return 1;
}

int     P3Pipe::SendFileData(RsFileData *item)
{
	SendAllItem(item);
	return 1;
}

int	P3Pipe::SendRsRawItem(RsRawItem *item)
{
	SendAllItem(item);
	return 1;
}

	// Cache Requests
RsCacheRequest *P3Pipe::RequestedSearch()
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	if (mRecvdRsCacheRequests.size() == 0)
	{
		return NULL;
	}

	RsCacheRequest *item = mRecvdRsCacheRequests.front();
	mRecvdRsCacheRequests.pop_front();
	
	return item;
}


	// Cache Results
RsCacheItem *P3Pipe::GetSearchResult()
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	if (mRecvdRsCacheItems.size() == 0)
	{
		return NULL;
	}

	RsCacheItem *item = mRecvdRsCacheItems.front();
	mRecvdRsCacheItems.pop_front();
	
	return item;
}


	// FileTransfer.
RsFileRequest *P3Pipe::GetFileRequest()
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	if (mRecvdRsFileRequests.size() == 0)
	{
		return NULL;
	}

	RsFileRequest *item = mRecvdRsFileRequests.front();
	mRecvdRsFileRequests.pop_front();
	
	return item;
}


RsFileData *P3Pipe::GetFileData()
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	if (mRecvdRsFileDatas.size() == 0)
	{
		return NULL;
	}

	RsFileData *item = mRecvdRsFileDatas.front();
	mRecvdRsFileDatas.pop_front();
	
	return item;
}


RsRawItem *P3Pipe::GetRsRawItem()
{
	RsStackMutex stack(pipeMtx); /***** LOCK MUTEX ****/

	if (mRecvdRsRawItems.size() == 0)
	{
		return NULL;
	}

	RsRawItem *item = mRecvdRsRawItems.front();
	mRecvdRsRawItems.pop_front();
	
	return item;
}

