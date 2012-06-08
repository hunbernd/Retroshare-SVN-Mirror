#ifndef RETROSHARE_IDENTITY_GUI_INTERFACE_H
#define RETROSHARE_IDENTITY_GUI_INTERFACE_H

/*
 * libretroshare/src/retroshare: rsidentity.h
 *
 * RetroShare C++ Interface.
 *
 * Copyright 2012-2012 by Robert Fernie.
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

#include <inttypes.h>
#include <string>
#include <list>


/********** Generic Token Request Interface ***********************
 * This is packaged here, as most TokenServices will require ID Services too.
 * The requests can be generic, but the reponses are service specific (dependent on data types).
 */

class RsTokenService
{
	public:

	RsTokenService()  { return; }
virtual ~RsTokenService() { return; }

        /* Data Requests */
virtual bool requestGroupList(uint32_t &token) = 0;
virtual bool requestGroupData(uint32_t &token, const std::list<std::string> &ids) = 0;
virtual bool requestMsgList(uint32_t &token, const std::list<std::string> &ids) = 0;
virtual bool requestMsgData(uint32_t &token, const std::list<std::string> &ids) = 0;

        /* Poll */
virtual uint32_t requestStatus(const uint32_t token) = 0;

};




/* The Main Interface Class - for information about your Peers */
class RsIdentity;
extern RsIdentity *rsIdentity;

#define RSID_TYPE_MASK		0xff00
#define RSID_RELATION_MASK	0x00ff

#define RSID_TYPE_REALID	0x0100
#define RSID_TYPE_PSEUDONYM	0x0200

#define RSID_RELATION_YOURSELF  0x0001
#define RSID_RELATION_FRIEND	0x0002
#define RSID_RELATION_FOF	0x0004
#define RSID_RELATION_OTHER   	0x0008
#define RSID_RELATION_UNKNOWN 	0x0010

std::string rsIdTypeToString(uint32_t idtype);

class RsIdData
{
	public:

	std::string mNickname;
	std::string mKeyId;

	uint32_t mIdType;

	std::string mGpgIdHash; // SHA(KeyId + Gpg Fingerprint) -> can only be IDed if GPG known.

	bool mGpgIdKnown; // if GpgIdHash has been identified.
	std::string mGpgId;   	// if known.
	std::string mGpgName; 	// if known.
	std::string mGpgEmail; 	// if known.
};

class RsIdReputation
{
	public:
	std::string mKeyId;

	int mYourRating;
	int mPeersRating;
	int mFofRating;
	int mTotalRating;

	std::string mComment;
};

class RsIdOpinion
{
	public:

	std::string mKeyId;
	std::string mPeerId;

	int mRating;
	int mPeersRating;
	std::string mComment;
};


class RsIdentity
{
	public:

	RsIdentity()  { return; }
virtual ~RsIdentity() { return; }


        /* changed? */
virtual bool updated() = 0;

        /* Interface now a request / poll / answer system */

        /* Data Requests */
virtual bool requestIdentityList(uint32_t &token) = 0;
virtual bool requestIdentities(uint32_t &token, const std::list<std::string> &ids) = 0;
virtual bool requestIdReputations(uint32_t &token, const std::list<std::string> &ids) = 0;
virtual bool requestIdPeerOpinion(uint32_t &token, const std::string &aboutId, const std::string &peerId) = 0;
//virtual bool requestIdGpgDetails(uint32_t &token, const std::list<std::string> &ids) = 0;

        /* Poll */
virtual uint32_t requestStatus(const uint32_t token) = 0;

        /* Retrieve Data */
virtual bool getIdentityList(const uint32_t token, std::list<std::string> &ids) = 0;
virtual bool getIdentity(const uint32_t token, RsIdData &data) = 0;
virtual bool getIdReputation(const uint32_t token, RsIdReputation &reputation) = 0;
virtual bool getIdPeerOpinion(const uint32_t token, RsIdOpinion &opinion) = 0;

        /* Updates */
virtual bool updateIdentity(RsIdData &data) = 0;
virtual bool updateOpinion(RsIdOpinion &opinion) = 0;

	/* TEMP */
virtual void generateDummyData() = 0;
};



#endif