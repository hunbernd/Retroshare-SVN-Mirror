/*
 * tcponudp/udprelay.cc
 *
 * libretroshare.
 *
 * Copyright 2010 by Robert Fernie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License Version 3 as published by the Free Software Foundation.
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

#include "udprelay.h"
#include <iostream>

/*
 * #define DEBUG_UDP_RELAY 1
 */

//#define DEBUG_UDP_RELAY 1


#ifdef DEBUG_UDP_RELAY
// DEBUG FUNCTION
#include <sstream>
#include <iomanip>
int displayUdpRelayPacketHeader(const void *data, const int size);
#endif

/****************** UDP RELAY STUFF **********/

#define MAX_RELAY_UDP_PACKET_SIZE 1024

UdpRelayReceiver::UdpRelayReceiver(UdpPublisher *pub)
	:UdpSubReceiver(pub)
{
	mClassLimit.resize(UDP_RELAY_NUM_CLASS);
	mClassCount.resize(UDP_RELAY_NUM_CLASS);

	setRelayTotal(UDP_RELAY_DEFAULT_COUNT_ALL);

	for(int i = 0; i < UDP_RELAY_NUM_CLASS; i++)
	{
		mClassCount[i] = 0;
	}

	/* only allocate this space once */
	mTmpSendPkt = malloc(MAX_RELAY_UDP_PACKET_SIZE);
	mTmpSendSize = MAX_RELAY_UDP_PACKET_SIZE;

	return;
}

UdpRelayReceiver::~UdpRelayReceiver()
{
	free(mTmpSendPkt);
}


int     UdpRelayReceiver::addUdpPeer(UdpPeer *peer, UdpRelayAddrSet *endPoints, const struct sockaddr_in *proxyaddr)
{
	struct sockaddr_in realPeerAddr = endPoints->mDestAddr;
	{
		RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/
		
		/* check for duplicate */
		std::map<struct sockaddr_in, UdpRelayEnd>::iterator it;
		it = mStreams.find(realPeerAddr);
		bool ok = (it == mStreams.end());
		if (!ok)
		{
	#ifdef DEBUG_UDP_RELAY
			std::cerr << "UdpPeerReceiver::addUdpPeer() ERROR Peer already exists!" << std::endl;
	#endif
			return 0;
		}
		
		/* setup a peer */
		UdpRelayEnd ure(endPoints, proxyaddr);
		
		mStreams[realPeerAddr] = ure;
	}
		
	{
		RsStackMutex stack(udppeerMtx);   /********** LOCK MUTEX *********/
		

#ifdef DEBUG_UDP_RELAY
		std::cerr << "UdpPeerReceiver::addUdpPeer() Just installing UdpPeer!" << std::endl;
#endif
		
		/* just overwrite */
		mPeers[realPeerAddr] = peer;
	}
		
	return 1;
}


int     UdpRelayReceiver::removeUdpPeer(UdpPeer *peer)
{
	bool found = false;
	struct sockaddr_in realPeerAddr;

	/* cleanup UdpPeer, and get reference for data */
	{
		RsStackMutex stack(udppeerMtx);   /********** LOCK MUTEX *********/
		
		std::map<struct sockaddr_in, UdpPeer *>::iterator it;
		for(it = mPeers.begin(); it != mPeers.end(); it++)
		{
			if (it->second == peer)
			{
				mPeers.erase(it);
				found = true;
				realPeerAddr = it->first;

				break;
			}
		}
	}

	if (!found)
	{
		return 0;
	}

	/* now we cleanup the associated data */
	{
		RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/
		
		std::map<struct sockaddr_in, UdpRelayEnd>::iterator it;
		it = mStreams.find(realPeerAddr);
		if (it != mStreams.end())
		{
			mStreams.erase(it);
		}
		else
		{
			/* ERROR */	
		}
	}
	return 1;
}

int UdpRelayReceiver::getRelayEnds(std::list<UdpRelayEnd> &relayEnds)
{
        RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	std::map<struct sockaddr_in, UdpRelayEnd>::iterator rit;
	
	for(rit = mStreams.begin(); rit != mStreams.end(); rit++)
	{
		relayEnds.push_back(rit->second);
	}
	return 1;


}

int UdpRelayReceiver::getRelayProxies(std::list<UdpRelayProxy> &relayProxies)
{
        RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	std::map<UdpRelayAddrSet, UdpRelayProxy>::iterator rit;
	
	for(rit = mRelays.begin(); rit != mRelays.end(); rit++)
	{
		relayProxies.push_back(rit->second);
	}
	return 1;
}

#define RELAY_MAX_BANDWIDTH 1000
#define RELAY_TIMEOUT		30


int UdpRelayReceiver::checkRelays()
{
        RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	/* iterate through the Relays */

	std::cerr << "UdpRelayReceiver::checkRelays()";
	std::cerr << std::endl;

	std::list<UdpRelayAddrSet> eraseList;
	std::map<UdpRelayAddrSet, UdpRelayProxy>::iterator rit;
	time_t now = time(NULL);
	
	for(rit = mRelays.begin(); rit != mRelays.end(); rit++)
	{
		/* calc bandwidth */
		rit->second.mBandwidth = rit->second.mDataSize / (float) (now - rit->second.mLastBandwidthTS);
		rit->second.mDataSize = 0;
		rit->second.mLastBandwidthTS = now;

		std::cerr << "UdpRelayReceiver::checkRelays()";
		std::cerr << "Relay: " << rit->first;
		std::cerr << " using bandwidth: " << rit->second.mBandwidth;
		std::cerr << std::endl;

		if (rit->second.mBandwidth > RELAY_MAX_BANDWIDTH)
		{
			std::cerr << "UdpRelayReceiver::checkRelays()";
			std::cerr << "Dropping Relay due to excessive Bandwidth: " << rit->first;
			std::cerr << std::endl;

			/* if exceeding bandwidth -> drop */
			eraseList.push_back(rit->first);
		}
		else if (now - rit->second.mLastTS > RELAY_TIMEOUT)
		{
			/* if haven't transmitted for ages -> drop */
			std::cerr << "UdpRelayReceiver::checkRelays()";
			std::cerr << "Dropping Relay due to Timeout: " << rit->first;
			std::cerr << std::endl;
			eraseList.push_back(rit->first);
		}
	}

	std::list<UdpRelayAddrSet>::iterator it;
	for(it = eraseList.begin(); it != eraseList.end(); it++)
	{
		removeUdpRelay_relayLocked(&(*it));
	}
	return 1;
}


int UdpRelayReceiver::removeUdpRelay(UdpRelayAddrSet *addrSet)
{
	RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	return removeUdpRelay_relayLocked(addrSet);
}


int UdpRelayReceiver::addUdpRelay(UdpRelayAddrSet *addrSet, int relayClass)
{
	RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	/* check for duplicate */
	std::map<UdpRelayAddrSet, UdpRelayProxy>::iterator rit = mRelays.find(*addrSet);
	int ok = (rit == mRelays.end());
	if (!ok)
	{
//#ifdef DEBUG_UDP_RELAY
		std::cerr << "UdpRelayReceiver::addUdpRelay() ERROR Peer already exists!" << std::endl;
//#endif
		return 0;
	}

	/* will install if there is space! */
	if (installRelayClass_relayLocked(relayClass))
	{
//#ifdef DEBUG_UDP_RELAY
		std::cerr << "UdpRelayReceiver::addUdpRelay() adding Relay" << std::endl;
//#endif
		/* create UdpRelay */
		UdpRelayProxy udpRelay(addrSet, relayClass);
		UdpRelayAddrSet alt = addrSet->flippedSet();
		UdpRelayProxy altUdpRelay(&alt, relayClass);

		/* must install two (A, B) & (B, A) */
		mRelays[*addrSet] = udpRelay;
		mRelays[alt] = altUdpRelay;

		return 1;
	}

//#ifdef DEBUG_UDP_RELAY
	std::cerr << "UdpRelayReceiver::addUdpRelay() ERROR Too many Relays!" << std::endl;
//#endif
	return 0;
}



int UdpRelayReceiver::removeUdpRelay_relayLocked(UdpRelayAddrSet *addrSet)
{
	/* find in Relay list */
        std::map<UdpRelayAddrSet, UdpRelayProxy>::iterator rit = mRelays.find(*addrSet);
	if (rit == mRelays.end())
	{
		/* ERROR */
		std::cerr << "UdpRelayReceiver::removeUdpRelay()";
		std::cerr << "ERROR Finding Relay: " << *addrSet;
		std::cerr << std::endl;
	}
	else
	{
		/* lets drop the count here too */
		removeRelayClass_relayLocked(rit->second.mRelayClass);
		mRelays.erase(rit);
	}

	/* rotate around and delete matching set */
	UdpRelayAddrSet alt = addrSet->flippedSet();

	rit = mRelays.find(alt);
	if (rit == mRelays.end())
	{
		std::cerr << "UdpRelayReceiver::removeUdpRelay()";
		std::cerr << "Error Finding Alt Relay: " << alt;
		std::cerr << std::endl;
		/* ERROR */
	}
	else
	{
		mRelays.erase(rit);
	}
	return 1;
}

        /* Need some stats, to work out how many relays we are supporting */
int UdpRelayReceiver::installRelayClass_relayLocked(int classIdx)
{
	/* check for total number of Relays */
	if (mClassCount[UDP_RELAY_CLASS_ALL] >= mClassLimit[UDP_RELAY_CLASS_ALL])
	{
		std::cerr << "UdpRelayReceiver::installRelayClass() ERROR Too many Relays already";
		std::cerr << std::endl;
		return 0;
	}

	/* check the idx too */
	if ((classIdx < 0) || (classIdx >= UDP_RELAY_NUM_CLASS))
	{
		std::cerr << "UdpRelayReceiver::installRelayClass() ERROR class Idx invalid";
		std::cerr << std::endl;
		return 0;
	}	

	/* now check the specifics of the class */
	if (mClassCount[classIdx] >= mClassLimit[classIdx])
	{
		std::cerr << "UdpRelayReceiver::installRelayClass() ERROR Relay Class Limit Exceeded";
		std::cerr << std::endl;

		return 0;
	}

	std::cerr << "UdpRelayReceiver::installRelayClass() Relay Class Ok, Count incremented";
	std::cerr << std::endl;

	/* if we get here we can add one */
	mClassCount[UDP_RELAY_CLASS_ALL]++;
	mClassCount[classIdx]++;

	return 1;
}

int UdpRelayReceiver::removeRelayClass_relayLocked(int classIdx)
{
	/* check for total number of Relays */
	if (mClassCount[UDP_RELAY_CLASS_ALL] < 1)
	{
		std::cerr << "UdpRelayReceiver::removeRelayClass() ERROR no relays installed";
		std::cerr << std::endl;
		return 0;
	}

	/* check the idx too */
	if ((classIdx < 0) || (classIdx >= UDP_RELAY_NUM_CLASS))
	{
		std::cerr << "UdpRelayReceiver::removeRelayClass() ERROR class Idx invalid";
		std::cerr << std::endl;
		return 0;
	}	

	/* now check the specifics of the class */
	if (mClassCount[classIdx] < 1)
	{
		std::cerr << "UdpRelayReceiver::removeRelayClass() ERROR no relay of class installed";
		std::cerr << std::endl;

		return 0;
	}

	std::cerr << "UdpRelayReceiver::removeRelayClass() Ok, Count decremented";
	std::cerr << std::endl;

	/* if we get here we can add one */
	mClassCount[UDP_RELAY_CLASS_ALL]--;
	mClassCount[classIdx]--;

	return 1;
}


int UdpRelayReceiver::setRelayTotal(int count)
{
	RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	mClassLimit[UDP_RELAY_CLASS_ALL] = count;
	mClassLimit[UDP_RELAY_CLASS_GENERAL] 	= (int)  (UDP_RELAY_FRAC_GENERAL * count);
	mClassLimit[UDP_RELAY_CLASS_FOF] 	= (int)  (UDP_RELAY_FRAC_FOF * count);
	mClassLimit[UDP_RELAY_CLASS_FRIENDS] 	= (int)  (UDP_RELAY_FRAC_FRIENDS * count);

	return count;
}


int UdpRelayReceiver::setRelayClassMax(int classIdx, int count)
{
	RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	/* check the idx */
	if ((classIdx < 0) || (classIdx >= UDP_RELAY_NUM_CLASS))
	{
		std::cerr << "UdpRelayReceiver::setRelayMaximum() ERROR class Idx invalid";
		std::cerr << std::endl;
		return 0;
	}	

	mClassLimit[classIdx] = count;
	return 1;
}


int UdpRelayReceiver::getRelayClassMax(int classIdx)
{
	RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	/* check the idx */
	if ((classIdx < 0) || (classIdx >= UDP_RELAY_NUM_CLASS))
	{
		std::cerr << "UdpRelayReceiver::getRelayMaximum() ERROR class Idx invalid";
		std::cerr << std::endl;
		return 0;
	}	

	return mClassLimit[classIdx];
}

int UdpRelayReceiver::getRelayCount(int classIdx)
{
	RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	/* check the idx */
	if ((classIdx < 0) || (classIdx >= UDP_RELAY_NUM_CLASS))
	{
		std::cerr << "UdpRelayReceiver::getRelayCount() ERROR class Idx invalid";
		std::cerr << std::endl;
		return 0;
	}	

	return mClassCount[classIdx];
}


int UdpRelayReceiver::RelayStatus(std::ostream &out)
{
        RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	/* iterate through the Relays */
	out << "UdpRelayReceiver::RelayStatus()";
	out << std::endl;

	std::map<UdpRelayAddrSet, UdpRelayProxy>::iterator rit;
	for(rit = mRelays.begin(); rit != mRelays.end(); rit++)
	{
		out << "Relay for: " << rit->first;
		out << std::endl;

		out << "\tClass: " << rit->second.mRelayClass;
		out << "\tBandwidth: " << rit->second.mBandwidth;
		out << "\tDataSize: " << rit->second.mDataSize;
		out << "\tLastBandwidthTS: " << rit->second.mLastBandwidthTS;
	}
	return 1;
}
	
int     UdpRelayReceiver::status(std::ostream &out)
{

	out << "UdpRelayReceiver::status()" << std::endl;
	out << "UdpRelayReceiver::Relayed Connections:" << std::endl;

	RelayStatus(out);

	RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/

	out << "UdpRelayReceiver::Connections:" << std::endl;

	std::map<struct sockaddr_in, UdpRelayEnd>::iterator pit;
	for(pit = mStreams.begin(); pit != mStreams.end(); pit++)
	{
		out << "\t" << pit->first << " : " << pit->second;
		out << std::endl;
	}

	return 1;
}

#define UDP_RELAY_HEADER_SIZE 16

/* higher level interface */
int UdpRelayReceiver::recvPkt(void *data, int size, struct sockaddr_in &from)
{
	/* print packet information */
#ifdef DEBUG_UDP_RELAY
	std::cerr << "UdpRelayReceiver::recvPkt(" << size << ") from: " << from;
	std::cerr << std::endl;
	displayUdpRelayPacketHeader(data, size);

#endif
 
	if (!isUdpRelayPacket(data, size))
	{
#ifdef DEBUG_UDP_RELAY
		std::cerr << "UdpRelayReceiver::recvPkt() is Not RELAY Pkt";
		std::cerr << std::endl;
#endif
		return 0;
	}

	/* decide if we are the relay, or the endpoint */
	UdpRelayAddrSet addrSet;
	if (!extractUdpRelayAddrSet(data, size, addrSet))
	{
		/* fails most basic test, drop */
		return 0; 
	}

	{
	        RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/
	
		/* lookup relay first (double entries) */
		std::map<UdpRelayAddrSet, UdpRelayProxy>::iterator rit = mRelays.find(addrSet);
		if (rit != mRelays.end())
		{
			/* we are the relay */
	#ifdef DEBUG_UDP_RELAY
			std::cerr << "UdpRelayReceiver::recvPkt() We are the Relay. Passing onto: ";
			std::cerr << rit->first.mDestAddr;
			std::cerr << std::endl;
	#endif
			/* do accounting */
			rit->second.mLastTS = time(NULL);
			rit->second.mDataSize += size;
	
			mPublisher->sendPkt(data, size, rit->first.mDestAddr, STD_RELAY_TTL);
			return 1;
		}
	}

	/* otherwise we are likely to be the endpoint,
	 * use the peers Address from the header
	 */

	{
	        RsStackMutex stack(udppeerMtx);   /********** LOCK MUTEX *********/

		std::map<struct sockaddr_in, UdpPeer *>::iterator pit = mPeers.find(addrSet.mSrcAddr);
		if (pit != mPeers.end())
		{
			/* we are the end-point */
	#ifdef DEBUG_UDP_RELAY
			std::cerr << "UdpRelayReceiver::recvPkt() Sending to UdpPeer: ";
			std::cerr << pit->first;
			std::cerr << std::endl;
	#endif
			/* remove the header */
			void *pktdata = (void *) (((uint8_t *) data) + UDP_RELAY_HEADER_SIZE);
			int   pktsize = size - UDP_RELAY_HEADER_SIZE;
			if (pktsize > 0)
			{
				(pit->second)->recvPkt(pktdata, pktsize);
			}
			else
			{
				/* packet undersized */
	#ifdef DEBUG_UDP_RELAY
				std::cerr << "UdpRelayReceiver::recvPkt() ERROR Packet Undersized";
				std::cerr << std::endl;
	#endif
			}
			return 1;
		}
		/* done */
	}

	/* unknown */
#ifdef DEBUG_UDP_RELAY
	std::cerr << "UdpRelayReceiver::recvPkt() Peer Unknown!";
	std::cerr << std::endl;
#endif
	return 0;
}


/* the address here must be the end point!, 
 * it cannot be proxy, as we could be using the same proxy for multiple connections. 
 */
int UdpRelayReceiver::sendPkt(const void *data, int size, const struct sockaddr_in &to, int ttl)
{
	RsStackMutex stack(relayMtx);   /********** LOCK MUTEX *********/
	
	/* work out who the proxy is */
	std::map<struct sockaddr_in, UdpRelayEnd>::iterator it;
	it = mStreams.find(to);
	if (it == mStreams.end())
	{
//#ifdef DEBUG_UDP_RELAY
		std::cerr << "UdpRelayReceiver::sendPkt() Peer Unknown!";
		std::cerr << std::endl;
//#endif
		return 0;
	}
	
	/* add a header to packet */
	int finalPktSize = createRelayUdpPacket(data, size, mTmpSendPkt, MAX_RELAY_UDP_PACKET_SIZE, &(it->second));

	/* send the packet on */
	return mPublisher->sendPkt(mTmpSendPkt, finalPktSize, it->second.mProxyAddr, STD_RELAY_TTL);
}

/***** RELAY PACKET FORMAT ****************************
 *
 *
 * [   0    |     1    |     2    |    3     ]
 *
 * [  'R'        'L'        'Y'     Version  ]
 * [     IP         Address      1           ]
 * [  Port       1    ][ IP   Address  2 ....
 * ... IP   Address  2][ Port 2              ]
 * [.... TUNNELLED DATA ....
 *
 *
 *      ... ]
 *
 * 16 Bytes: 4 ID, 6 IP:Port 1, 6 IP:Port 2 
 */

#define UDP_IDENTITY_STRING_V1	"RLY1"
#define UDP_IDENTITY_SIZE_V1	4

int isUdpRelayPacket(const void *data, const int size)
{
	if (size < UDP_RELAY_HEADER_SIZE)
		return 0;
	
	return (0 == strncmp((char *) data, UDP_IDENTITY_STRING_V1, UDP_IDENTITY_SIZE_V1));
}

#ifdef DEBUG_UDP_RELAY

int displayUdpRelayPacketHeader(const void *data, const int size)
{
	int dsize = UDP_RELAY_HEADER_SIZE + 16;
	if (size < dsize)
	{
		dsize = size;
	}

	std::ostringstream out;
        for(int i = 0; i < dsize; i++)
        {
		if ((i > 0) && (i % 16 == 0))
		{
			out << std::endl;
		}

                out << std::setw(2) << std::setfill('0') << std::hex << (uint32_t) ((uint8_t*) data)[i];
        }

	std::cerr << "displayUdpRelayPacketHeader()" << std::endl;
	std::cerr << out.str();
	std::cerr << std::endl;

}
#endif


int extractUdpRelayAddrSet(const void *data, const int size, UdpRelayAddrSet &addrSet)
{
	if (size < UDP_RELAY_HEADER_SIZE)
	{
		std::cerr << "createRelayUdpPacket() ERROR invalid size";
		std::cerr << std::endl;
		return 0;
	}

	uint8_t *header = (uint8_t *) data;

	sockaddr_clear(&(addrSet.mSrcAddr));
	sockaddr_clear(&(addrSet.mDestAddr));

	/* as IP:Port are already in network byte order, we can just write them to the dataspace */
	uint32_t ipaddr;
	uint16_t port;

	memcpy(&ipaddr, &(header[4]), 4);	
	memcpy(&port, &(header[8]), 2);	

 	addrSet.mSrcAddr.sin_addr.s_addr = ipaddr;
 	addrSet.mSrcAddr.sin_port = port;

	memcpy(&ipaddr, &(header[10]), 4);	
	memcpy(&port, &(header[14]), 2);	

 	addrSet.mDestAddr.sin_addr.s_addr = ipaddr;
 	addrSet.mDestAddr.sin_port = port;

	return 1;
}


int createRelayUdpPacket(const void *data, const int size, void *newpkt, int newsize, UdpRelayEnd *ure)
{
	int pktsize = size + UDP_RELAY_HEADER_SIZE;
	if (newsize < pktsize)
	{
		std::cerr << "createRelayUdpPacket() ERROR invalid size";
		std::cerr << std::endl;
		return 0;
	}
	uint8_t *header = (uint8_t *) newpkt;
	memcpy(header, UDP_IDENTITY_STRING_V1, UDP_IDENTITY_SIZE_V1);

	/* as IP:Port are already in network byte order, we can just write them to the dataspace */
	uint32_t ipaddr = ure->mLocalAddr.sin_addr.s_addr;
	uint16_t port = ure->mLocalAddr.sin_port;

	memcpy(&(header[4]), &ipaddr, 4);	
	memcpy(&(header[8]), &port, 2);	

	ipaddr = ure->mRemoteAddr.sin_addr.s_addr;
	port = ure->mRemoteAddr.sin_port;

	memcpy(&(header[10]), &ipaddr, 4);	
	memcpy(&(header[14]), &port, 2);	

	memcpy(&(header[16]), data, size);

	return pktsize;
}



/******* Small Container Class Helper Functions ****/

UdpRelayAddrSet::UdpRelayAddrSet()
{
	sockaddr_clear(&mSrcAddr);
	sockaddr_clear(&mDestAddr);
}

UdpRelayAddrSet::UdpRelayAddrSet(const sockaddr_in *ownAddr, const sockaddr_in *destAddr)
{
	mSrcAddr = *ownAddr;
	mDestAddr = *destAddr;
}

UdpRelayAddrSet UdpRelayAddrSet::flippedSet()
{	
	UdpRelayAddrSet flipped(&mDestAddr, &mSrcAddr);
	return flipped;
}


int operator<(const UdpRelayAddrSet &a, const UdpRelayAddrSet &b)
{
	if (a.mSrcAddr < b.mSrcAddr)
	{
		return 1;
	}

	if (a.mSrcAddr == b.mSrcAddr)
	{
		if (a.mDestAddr < b.mDestAddr)
		{
			return 1;
		}
	}
	return 0;
}


UdpRelayProxy::UdpRelayProxy()
{
	mBandwidth = 0;
	mDataSize = 0;
	mLastBandwidthTS = 0;
	mLastTS = 0;
	mRelayClass = 0;
}

UdpRelayProxy::UdpRelayProxy(UdpRelayAddrSet *addrSet, int relayClass)
{
	mAddrs = *addrSet;
	mRelayClass = relayClass;

	mBandwidth = 0;
	mDataSize = 0;
	mLastBandwidthTS = 0;
	mLastTS = 0;
}

UdpRelayEnd::UdpRelayEnd() 
{ 
	sockaddr_clear(&mLocalAddr);
	sockaddr_clear(&mProxyAddr);
	sockaddr_clear(&mRemoteAddr);

}

UdpRelayEnd::UdpRelayEnd(UdpRelayAddrSet *endPoints, const struct sockaddr_in *proxyaddr)
{
	mLocalAddr = endPoints->mSrcAddr;
	mRemoteAddr = endPoints->mDestAddr;
	mProxyAddr = *proxyaddr;
}





std::ostream &operator<<(std::ostream &out, const UdpRelayAddrSet &uras)
{
	out << "<" << uras.mSrcAddr << "," << uras.mDestAddr << ">";
	return out;
}

std::ostream &operator<<(std::ostream &out, const UdpRelayProxy &urp)
{
	time_t now = time(NULL);
	out << "UdpRelayProxy for " << urp.mAddrs;
	out << std::endl;
	out << "\tRelayClass: " << urp.mRelayClass;
	out << std::endl;
	out << "\tBandwidth: " << urp.mBandwidth;
	out << std::endl;
	out << "\tDataSize: " << urp.mDataSize;
	out << std::endl;
	out << "\tLastBandwidthTS: " << now - urp.mLastBandwidthTS << " secs ago";
	out << std::endl;
	out << "\tLastTS: " << now - urp.mLastTS << " secs ago";
	out << std::endl;

	return out;
}

std::ostream &operator<<(std::ostream &out, const UdpRelayEnd &ure)
{
	out << "UdpRelayEnd: <" << ure.mLocalAddr << " => " << ure.mProxyAddr << " <= ";
	out << ure.mRemoteAddr << ">";
	return out;
}




