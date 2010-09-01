
/*
 * "$Id: p3face-msgs.cc,v 1.7 2007-05-05 16:10:06 rmf24 Exp $"
 *
 * RetroShare C++ Interface.
 *
 * Copyright 2004-2006 by Robert Fernie.
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



#include <iostream>
#include <sstream>

#include "util/rsdir.h"
#include "util/rsdebug.h"
const int p3facemsgzone = 11453;

#include <sys/time.h>
#include <time.h>

#include "retroshare/rstypes.h"
#include "rsserver/p3msgs.h"

#include "services/p3msgservice.h"
#include "services/p3chatservice.h"

#include "pqi/authgpg.h"

/* external reference point */
RsMsgs *rsMsgs = NULL;

/****************************************/
/****************************************/

bool p3Msgs::getMessageSummaries(std::list<MsgInfoSummary> &msgList)
{
	return mMsgSrv->getMessageSummaries(msgList);
}



bool p3Msgs::getMessage(std::string mid, MessageInfo &msg)
{
	return mMsgSrv->getMessage(mid, msg);
}

void p3Msgs::getMessageCount(unsigned int *pnInbox, unsigned int *pnInboxNew, unsigned int *pnOutbox, unsigned int *pnDraftbox, unsigned int *pnSentbox, unsigned int *pnTrashbox)
{
	mMsgSrv->getMessageCount(pnInbox, pnInboxNew, pnOutbox, pnDraftbox, pnSentbox, pnTrashbox);
}

/****************************************/
/****************************************/
	/* Message Items */
bool p3Msgs::MessageSend(MessageInfo &info)
{
	return mMsgSrv->MessageSend(info);
}

bool p3Msgs::MessageToDraft(MessageInfo &info)
{
    return mMsgSrv->MessageToDraft(info);
}

bool p3Msgs::MessageToTrash(std::string mid, bool bTrash)
{
    return mMsgSrv->MessageToTrash(mid, bTrash);
}

/****************************************/
/****************************************/
bool p3Msgs::MessageDelete(std::string mid)
{
	//std::cerr << "p3Msgs::MessageDelete() ";
	//std::cerr << "mid: " << mid << std::endl;

	return mMsgSrv -> removeMsgId(mid);
}

bool p3Msgs::MessageRead(std::string mid, bool bUnreadByUser)
{
	//std::cerr << "p3Msgs::MessageRead() ";
	//std::cerr << "mid: " << mid << std::endl;

	return mMsgSrv -> markMsgIdRead(mid, bUnreadByUser);
}

bool 	p3Msgs::getMessageTagTypes(MsgTagType& tags)
{
	return mMsgSrv->getMessageTagTypes(tags);
}

bool  p3Msgs::setMessageTagType(uint32_t tagId, std::string& text, uint32_t rgb_color)
{
	return mMsgSrv->setMessageTagType(tagId, text, rgb_color);
}

bool    p3Msgs::removeMessageTagType(uint32_t tagId)
{
	return mMsgSrv->removeMessageTagType(tagId);
}

bool 	p3Msgs::getMessageTag(std::string msgId, MsgTagInfo& info)
{
	return mMsgSrv->getMessageTag(msgId, info);
}

bool 	p3Msgs::setMessageTag(std::string msgId, uint32_t tagId, bool set)
{
	return mMsgSrv->setMessageTag(msgId, tagId, set);
}

bool    p3Msgs::resetMessageStandardTagTypes(MsgTagType& tags)
{
	return mMsgSrv->resetMessageStandardTagTypes(tags);
}

/****************************************/
/****************************************/
bool 	p3Msgs::sendPublicChat(std::wstring msg)
{
	/* send a message to all for now */
	return mChatSrv -> sendPublicChat(msg);
}

bool 	p3Msgs::sendPrivateChat(std::string id, std::wstring msg)
{
	/* send a message to peer */
	return mChatSrv -> sendPrivateChat(id, msg);
}

void p3Msgs::sendGroupChatStatusString(const std::string& status_string) 
{
	mChatSrv->sendGroupChatStatusString(status_string);
}
void p3Msgs::sendStatusString(const std::string& peer_id,const std::string& status_string) 
{
	mChatSrv->sendStatusString(peer_id,status_string);
}

int	p3Msgs::getChatQueueCount(bool privateQueue)
{
	return mChatSrv->getChatQueueCount(privateQueue);
}

bool	p3Msgs::getPublicChatQueue(std::list<ChatInfo> &chats)
{
	return mChatSrv->getPublicChatQueue(chats);
}

bool   p3Msgs::getPrivateChatQueueIds(std::list<std::string> &ids)
{
	return mChatSrv->getPrivateChatQueueIds(ids);
}

bool	p3Msgs::getPrivateChatQueue(std::string id, std::list<ChatInfo> &chats)
{
	return mChatSrv->getPrivateChatQueue(id, chats);
}

void p3Msgs::getOwnAvatarData(unsigned char *& data,int& size)
{
	mChatSrv->getOwnAvatarJpegData(data,size) ;
}

void p3Msgs::setOwnAvatarData(const unsigned char *data,int size)
{
	mChatSrv->setOwnAvatarJpegData(data,size) ;
}

void p3Msgs::getAvatarData(std::string pid,unsigned char *& data,int& size)
{
	mChatSrv->getAvatarJpegData(pid,data,size) ;
}

std::string p3Msgs::getCustomStateString(const std::string& peer_id)
{
	return mChatSrv->getCustomStateString(peer_id) ;
}

std::string p3Msgs::getCustomStateString()
{
	return mChatSrv->getOwnCustomStateString() ;
}

void p3Msgs::setCustomStateString(const std::string& state_string)
{
	mChatSrv->setOwnCustomStateString(state_string) ;
}

