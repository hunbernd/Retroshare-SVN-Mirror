
/*
 * "$Id: rsdir.cc,v 1.1 2007-02-19 20:08:30 rmf24 Exp $"
 *
 * RetroShare C++ Interface.
 *
 * Copyright 2004-2007 by Robert Fernie.
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

// Includes for directory creation.
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/rsdir.h"
#include "pqi/pqinotify.h"
#include "retroshare/rstypes.h"
#include "rsthreads.h"
#include <string>
#include <iostream>
#include <algorithm>
#include <stdio.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>

#include <fstream>

#if defined(WIN32) || defined(__CYGWIN__)
#include "util/rswin.h"
#include "wtypes.h"
#include <winioctl.h>
#else
#include <errno.h>
#endif

/****
 * #define RSDIR_DEBUG 1
 ****/

std::string 	RsDirUtil::getTopDir(const std::string& dir)
{
	std::string top;

	/* find the subdir: [/][dir1.../]<top>[/]
	 */
	int i,j;
	int len = dir.length();
	for(j = len - 1; (j > 0) && (dir[j] == '/'); j--);
	for(i = j; (i > 0) && (dir[i] != '/'); i--);

	if (dir[i] == '/')
		i++;

	for(; i <= j; i++)
	{
		top += dir[i];
	}

	return top;
}

class CRC32Table
{
	public:
		inline uint32_t operator[](unsigned char i) const { return _data[i] ; }

		CRC32Table()
		{
			_data = new uint32_t[256] ;
			uint32_t polynmial = 0x04c11db7 ;

			for(uint32_t i=0;i<256;++i)
			{
				uint32_t a = reflect(i,8)<<24 ;

				for(uint32_t j=0;j<8;++j)
					a = (a << 1)^ ( (a&(1<<31))?polynmial:0) ;

				_data[i] = reflect(a,32) ;
			}
		}
		// Swap bits 0-7, 1-6, etc.
		//
		uint32_t reflect(uint32_t ref,unsigned char ch)
		{
			uint32_t val = 0 ;
			for(int i=1;i<ch+1;i++,ref>>=1)
				if(ref & 1)
					val |= (1 << (ch-i)) ;
			return val ;
		}
		~CRC32Table() { delete[] _data ; }

	private:
		uint32_t *_data ;
};

static const CRC32Table crc32_table ;

uint32_t RsDirUtil::rs_CRC32(const unsigned char *data,uint32_t _len) 
{
	uint32_t a = 0xffffffff ;
	int64_t len = _len ;

	for(const unsigned char *buf=data;len>0;len--)
		a = (a >> 8) ^ crc32_table[ (a & 0xff) ^ *buf++] ;

	return a ^ 0xffffffff ;
}

bool RsDirUtil::crc32File(FILE *fd, uint64_t file_size,uint32_t chunk_size, CRC32Map& crc_map) 
{
	if(fseeko64(fd,0,SEEK_SET) != 0)
	{
		std::cerr << "crc32File(): cannot fseek to beginnign of the file !!" << std::endl ;
		return false ;
	}

	crc_map = CRC32Map(file_size,chunk_size) ;

	unsigned char *buff = new unsigned char[chunk_size] ;
	int len ;
	uint64_t total_size = 0 ;
	uint32_t nb_chunk = 0;

	while((len = fread(buff,(size_t)1, (size_t)chunk_size, fd)) > 0)
	{
		crc_map.set(nb_chunk++,rs_CRC32(buff,len)) ;
		total_size += len ;
	}
	delete[] buff ;

	if(file_size != total_size)
	{
		std::cerr << "RsDirUtil::crc32File(): ERROR. The file size does not match the announced size." << std::endl ;
		return false ;
	}
	return true ;
}

std::string 	RsDirUtil::removeTopDir(const std::string& dir)
{
	std::string rest;

	/* remove the subdir: [/][dir1.../]<top>[/]
	 */
	int i,j;
	int len = dir.length();
	for(j = len - 1; (j > 0) && (dir[j] == '/'); j--);
	for(i = j; (i >= 0) && (dir[i] != '/'); i--);

	/* remove any more slashes */
	for(; (i >= 0) && (dir[i] == '/'); i--);

	for(j = 0; j <= i; j++)
	{
		rest += dir[j];
	}

	return rest;
}

std::string 	RsDirUtil::getRootDir(const std::string& dir)
{
	std::string root;

	/* find the subdir: [/]root[/...]
	 */
	int i,j;
	int len = dir.length();
	for(i = 0; (i < len) && (dir[i] == '/'); i++);
	for(j = i; (j < len) && (dir[j] != '/'); j++);
	if (i == j)
		return root; /* empty */
	for(; i < j; i++)
	{
		root += dir[i];
	}
	return root;
}

std::string RsDirUtil::removeRootDir(const std::string& path)
{
	unsigned int i, j;
	unsigned int len = path.length();
	std::string output;

	/* chew leading '/'s */
	for(i = 0; (i < len) && (path[i] == '/'); i++);
	if (i == len)
			return output; /*  empty string */

	for(j = i; (j < len) && (path[j] != '/'); j++); /* run to next '/' */
	for(; (j < len) && (path[j] == '/'); j++); 	/* chew leading '/'s */

	for(; j < len; j++)
	{
		output += path[j];
	}

	return output;
}

std::string RsDirUtil::removeRootDirs(const std::string& path, const std::string& root)
{
	/* too tired */
	std::string notroot;

	unsigned int i = 0, j = 0;

	/* catch empty data */
	if ((root.length() < 1) || (path.length() < 1))
		return notroot;
		
	if ((path[0] == '/') && (root[0] != '/'))
	{
		i++;
	}

	for(; (i < path.length()) && (j < root.length()) && (path[i] == root[j]); i++, j++);

	/* should have consumed root. */
	if (j == root.length())
	{
		//std::cerr << "matched root!" << std::endl;
	}
	else
	{
		//std::cerr << "failed i: " << i << ", j: " << j << std::endl;
		//std::cerr << "root: " << root << " path: " << path << std::endl;
		return notroot;
	}

	if (path[i] == '/')
	{
		i++;
	}

	for(; i < path.length(); i++)
	{
		notroot += path[i];
	}

	//std::cerr << "Found NotRoot: " << notroot << std::endl;

	return notroot;
}



int	RsDirUtil::breakupDirList(const std::string& path, 
			std::list<std::string> &subdirs)
{
	int start = 0;
	unsigned int i;
	for(i = 0; i < path.length(); i++)
	{
		if (path[i] == '/')
		{
			if (i - start > 0)
			{
				subdirs.push_back(path.substr(start, i-start));
			}
			start = i+1;
		}
	}
	// get the final one.
	if (i - start > 0)
	{
		subdirs.push_back(path.substr(start, i-start));
	}
	return 1;
}


/**** Copied and Tweaked from ftcontroller ***/
bool RsDirUtil::copyFile(const std::string& source,const std::string& dest)
{
#ifdef WINDOWS_SYS
        std::wstring sourceW;
        std::wstring destW;
        librs::util::ConvertUtf8ToUtf16(source,sourceW);
        librs::util::ConvertUtf8ToUtf16(dest,destW);

        return (CopyFileW(sourceW.c_str(), destW.c_str(), FALSE) != 0);

#else
	FILE *in = fopen64(source.c_str(),"rb") ;

	if(in == NULL)
	{
		return false ;
	}

	FILE *out = fopen64(dest.c_str(),"wb") ;

	if(out == NULL)
	{
		fclose (in);
		return false ;
	}

	size_t s=0;
	size_t T=0;

	static const int BUFF_SIZE = 10485760 ; // 10 MB buffer to speed things up.
	void *buffer = malloc(BUFF_SIZE) ;

	bool bRet = true;

	while( (s = fread(buffer,1,BUFF_SIZE,in)) > 0)
	{
		size_t t = fwrite(buffer,1,s,out) ;
		T += t ;

		if(t != s)
		{
			bRet = false ;
			break;
		}
	}

	fclose(in) ;
	fclose(out) ;

	free(buffer) ;

	return true ;

#endif

}


bool	RsDirUtil::checkFile(const std::string& filename)
{
	int val;
	mode_t st_mode;
#ifdef WINDOWS_SYS
	std::wstring wfilename;
	librs::util::ConvertUtf8ToUtf16(filename, wfilename);
	struct _stat buf;
	val = _wstat(wfilename.c_str(), &buf);
	st_mode = buf.st_mode;
#else
	struct stat buf;
	val = stat(filename.c_str(), &buf);
	st_mode = buf.st_mode;
#endif
	if (val == -1)
	{
#ifdef RSDIR_DEBUG 
		std::cerr << "RsDirUtil::checkFile() ";
		std::cerr << filename << " doesn't exist" << std::endl;
#endif
		return false;
	} 
	else if (!S_ISREG(st_mode))
	{
		// Some other type - error.
#ifdef RSDIR_DEBUG 
		std::cerr << "RsDirUtil::checkFile() ";
		std::cerr << filename << " is not a Regular File" << std::endl;
#endif
		return false;
	}
	return true;
}


bool	RsDirUtil::checkDirectory(const std::string& dir)
{
	int val;
	mode_t st_mode;
#ifdef WINDOWS_SYS
	std::wstring wdir;
	librs::util::ConvertUtf8ToUtf16(dir, wdir);
	struct _stat buf;
	val = _wstat(wdir.c_str(), &buf);
	st_mode = buf.st_mode;
#else
	struct stat buf;
	val = stat(dir.c_str(), &buf);
	st_mode = buf.st_mode;
#endif
	if (val == -1)
	{
#ifdef RSDIR_DEBUG 
		std::cerr << "RsDirUtil::checkDirectory() ";
		std::cerr << dir << " doesn't exist" << std::endl;
#endif
		return false;
	} 
	else if (!S_ISDIR(st_mode))
	{
		// Some other type - error.
#ifdef RSDIR_DEBUG 
		std::cerr << "RsDirUtil::checkDirectory() ";
		std::cerr << dir << " is not Directory" << std::endl;
#endif
		return false;
	}
	return true;
}


bool	RsDirUtil::checkCreateDirectory(const std::string& dir)
{
#ifdef RSDIR_DEBUG
	std::cerr << "RsDirUtil::checkCreateDirectory() dir: " << dir << std::endl;
#endif

#ifdef WINDOWS_SYS
	std::wstring wdir;
	librs::util::ConvertUtf8ToUtf16(dir, wdir);
	_WDIR *direc = _wopendir(wdir.c_str());
	if (!direc)
#else
	DIR *direc = opendir(dir.c_str());
	if (!direc)
#endif
	{
		// directory don't exist. create.
/******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // UNIX
		if (-1 == mkdir(dir.c_str(), 0777))
#else // WIN
		if (-1 == _wmkdir(wdir.c_str()))
#endif
/******************************** WINDOWS/UNIX SPECIFIC PART ******************/

		{
#ifdef RSDIR_DEBUG
			std::cerr << "check_create_directory() Fatal Error et oui--";
			std::cerr <<std::endl<< "\tcannot create:" <<dir<<std::endl;
#endif
			return 0;
		}

#ifdef RSDIR_DEBUG
		std::cerr << "check_create_directory()";
		std::cerr <<std::endl<< "\tcreated:" <<dir<<std::endl;
#endif

		return 1;
	}

#ifdef RSDIR_DEBUG
	std::cerr << "check_create_directory()";
	std::cerr <<std::endl<< "\tDir Exists:" <<dir<<std::endl;
#endif

#ifdef WINDOWS_SYS
	_wclosedir(direc);
#else
	closedir(direc) ;
#endif

	return 1;
}

bool 	RsDirUtil::cleanupDirectory(const std::string& cleandir, const std::list<std::string> &keepFiles)
{

	/* check for the dir existance */
#ifdef WINDOWS_SYS
	std::wstring wcleandir;
	librs::util::ConvertUtf8ToUtf16(cleandir, wcleandir);
	_WDIR *dir = _wopendir(wcleandir.c_str());
#else
	DIR *dir = opendir(cleandir.c_str());
#endif

	std::list<std::string>::const_iterator it;

	if (!dir)
	{
		return false;
	}

#ifdef WINDOWS_SYS
	struct _wdirent *dent;
	struct _stat buf;

	while(NULL != (dent = _wreaddir(dir)))
#else
	struct dirent *dent;
	struct stat buf;

	while(NULL != (dent = readdir(dir)))
#endif
	{
		/* check entry type */
#ifdef WINDOWS_SYS
		const std::wstring &wfname = dent -> d_name;
		std::wstring wfullname = wcleandir + L"/" + wfname;
#else
		const std::string &fname = dent -> d_name;
		std::string fullname = cleandir + "/" + fname;
#endif

#ifdef WINDOWS_SYS
		if (-1 != _wstat(wfullname.c_str(), &buf))
#else
		if (-1 != stat(fullname.c_str(), &buf))
#endif
		{
			/* only worry about files */
			if (S_ISREG(buf.st_mode))
			{
#ifdef WINDOWS_SYS
				std::string fname;
				librs::util::ConvertUtf16ToUtf8(wfname, fname);
#endif
				/* check if we should keep it */
				if (keepFiles.end() == (it = std::find(keepFiles.begin(), keepFiles.end(), fname)))
				{
					/* can remove */
#ifdef WINDOWS_SYS
					_wremove(wfullname.c_str());
#else
					remove(fullname.c_str());
#endif
				}
			}
		}
	}

	/* close directory */
#ifdef WINDOWS_SYS
	_wclosedir(dir);
#else
	closedir(dir);
#endif

	return true;
}

/* slightly nicer helper function */
bool RsDirUtil::hashFile(const std::string& filepath, 
		std::string &name, std::string &hash, uint64_t &size)
{
	if (getFileHash(filepath, hash, size))
	{
		/* extract file name */
		name = RsDirUtil::getTopDir(filepath);
		return true;
	}
	return false;
}


#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

/* Function to hash, and get details of a file */
bool RsDirUtil::getFileHash(const std::string& filepath, std::string &hash, uint64_t &size, RsThread *thread /*= NULL*/)
{
	FILE *fd;
	int  len;
	SHA_CTX *sha_ctx = new SHA_CTX;
	unsigned char sha_buf[SHA_DIGEST_LENGTH];
	unsigned char gblBuf[512];

#ifdef WINDOWS_SYS
	std::wstring filepathW;
	librs::util::ConvertUtf8ToUtf16(filepath, filepathW);
	if (NULL == (fd = _wfopen(filepathW.c_str(), L"rb")))
		return false;
#else
	if (NULL == (fd = fopen64(filepath.c_str(), "rb")))
		return false;
#endif

	/* determine size */
 	fseeko64(fd, 0, SEEK_END);
	size = ftello64(fd);
	fseeko64(fd, 0, SEEK_SET);

	/* check if thread is running */
	bool isRunning = thread ? thread->isRunning() : true;
	int runningCheckCount = 0;

	SHA1_Init(sha_ctx);
	while(isRunning && (len = fread(gblBuf,1, 512, fd)) > 0)
	{
		SHA1_Update(sha_ctx, gblBuf, len);

		if (thread && ++runningCheckCount > (10 * 1024)) {
			/* check all 50MB if thread is running */
			isRunning = thread->isRunning();
			runningCheckCount = 0;
		}
	}

	/* Thread has ended */
	if (isRunning == false)
	{
		delete sha_ctx;
		fclose(fd);
		return false;
	}

	/* reading failed for some reason */
	if (ferror(fd))
	{
		delete sha_ctx;
		fclose(fd);
		return false;
	}

	SHA1_Final(&sha_buf[0], sha_ctx);

	std::ostringstream tmpout;
	for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
	{
		tmpout << std::setw(2) << std::setfill('0') << std::hex << (unsigned int) (sha_buf[i]);
	}
	hash = tmpout.str();

	delete sha_ctx;
	fclose(fd);
	return true;
}

bool RsDirUtil::renameFile(const std::string& from, const std::string& to)
{
	int			loops = 0;

#if defined(WIN32) || defined(MINGW) || defined(__CYGWIN__)
#ifdef WIN_CROSS_UBUNTU
	std::wstring f,t ;
	for(int i=0;i<from.size();++i) f.push_back(from[i]) ;
	for(int i=0;i<to.size();++i) t.push_back(to[i]) ;
#else
	std::string f(from),t(to) ;
#endif
	while (!MoveFileExA(f.c_str(), t.c_str(), MOVEFILE_REPLACE_EXISTING))
#else
	while (rename(from.c_str(), to.c_str()) < 0)
#endif
	{
#ifdef WIN32
		if (GetLastError() != ERROR_ACCESS_DENIED)
#else
		if (errno != EACCES)
#endif
			/* set errno? */
			return false ;
#ifdef WIN32
		Sleep(100000);				/* us */
#else
		usleep(100000);				/* us */
#endif

		if (loops >= 30)
			return false ;

		loops++;
	}

	return true ;
}

bool RsDirUtil::createBackup (const std::string& sFilename, unsigned int nCount)
{
#ifdef WINDOWS_SYS
    if (GetFileAttributesA (sFilename.c_str ()) == -1) {
        // file doesn't exist
        return true;
    }

    // search last backup
    int nLast;
    for (nLast = nCount; nLast >= 1; nLast--) {
        std::ostringstream sStream;
        sStream << sFilename << nLast << ".bak";

        if (GetFileAttributesA (sStream.str ().c_str ()) != -1) {
            break;
        }
    }

    // delete max backup
    if (nLast == nCount) {
        std::ostringstream sStream;
        sStream << sFilename << nCount << ".bak";
        if (DeleteFileA (sStream.str ().c_str ()) == FALSE) {
            getPqiNotify()->AddSysMessage (0, RS_SYS_WARNING, "File delete error", "Error while deleting file " + sStream.str ());
            return false;
        }
        nLast--;
    }

    // rename backups
    for (int nIndex = nLast; nIndex >= 1; nIndex--) {
        std::ostringstream sStream;
        sStream << sFilename << nIndex << ".bak";
        std::ostringstream sStream1;
        sStream1 << sFilename << nIndex + 1 << ".bak";

        if (renameFile (sStream.str (), sStream1.str ()) == false) {
            getPqiNotify()->AddSysMessage (0, RS_SYS_WARNING, "File rename error", "Error while renaming file " + sStream.str () + " to " + sStream1.str ());
            return false;
        }
    }

    // copy backup
    std::ostringstream sStream;
    sStream << sFilename << 1 << ".bak";
    if (CopyFileA (sFilename.c_str (), sStream.str ().c_str (), FALSE) == FALSE) {
        getPqiNotify()->AddSysMessage (0, RS_SYS_WARNING, "File rename error", "Error while renaming file " + sFilename + " to " + sStream.str ());
        return false;
    }
#endif
    return true;
}

#if 0 // NOT ENABLED YET!
/************************* WIDE STRING ***************************/
/************************* WIDE STRING ***************************/
/************************* WIDE STRING ***************************/

std::wstring 	RsDirUtil::getWideTopDir(std::wstring dir)
{
	std::wstring top;

	/* find the subdir: [/][dir1.../]<top>[/]
	 */
	int i,j;
	int len = dir.length();
	for(j = len - 1; (j > 0) && (dir[j] == '/'); j--);
	for(i = j; (i > 0) && (dir[i] != '/'); i--);

	if (dir[i] == '/')
		i++;

	for(; i <= j; i++)
	{
		top += dir[i];
	}

	return top;
}

std::wstring 	RsDirUtil::removeWideTopDir(std::wstring dir)
{
	std::wstring rest;

	/* remove the subdir: [/][dir1.../]<top>[/]
	 */
	int i,j;
	int len = dir.length();
	for(j = len - 1; (j > 0) && (dir[j] == '/'); j--);
	for(i = j; (i >= 0) && (dir[i] != '/'); i--);

	/* remove any more slashes */
	for(; (i >= 0) && (dir[i] == '/'); i--);

	for(j = 0; j <= i; j++)
	{
		rest += dir[j];
	}

	return rest;
}

std::wstring 	RsDirUtil::getWideRootDir(std::wstring dir)
{
	std::wstring root;

	/* find the subdir: [/]root[/...]
	 */
	int i,j;
	int len = dir.length();
	for(i = 0; (i < len) && (dir[i] == '/'); i++);
	for(j = i; (j < len) && (dir[j] != '/'); j++);
	if (i == j)
		return root; /* empty */
	for(; i < j; i++)
	{
		root += dir[i];
	}
	return root;
}

std::wstring RsDirUtil::removeWideRootDir(std::wstring path)
{
	unsigned int i, j;
	unsigned int len = path.length();
	std::wstring output;

	/* chew leading '/'s */
	for(i = 0; (i < len) && (path[i] == '/'); i++);
	if (i == len)
			return output; /*  empty string */

	for(j = i; (j < len) && (path[j] != '/'); j++); /* run to next '/' */
	for(; (j < len) && (path[j] == '/'); j++); 	/* chew leading '/'s */

	for(; j < len; j++)
	{
		output += path[j];
	}

	return output;
}

std::wstring RsDirUtil::removeWideRootDirs(std::wstring path, std::wstring root)
{
	/* too tired */
	std::wstring notroot;

	unsigned int i = 0, j = 0;

	/* catch empty data */
	if ((root.length() < 1) || (path.length() < 1))
		return notroot;
		
	if ((path[0] == '/') && (root[0] != '/'))
	{
		i++;
	}

	for(; (i < path.length()) && (j < root.length()) && (path[i] == root[j]); i++, j++);

	/* should have consumed root. */
	if (j == root.length())
	{
		//std::cerr << "matched root!" << std::endl;
	}
	else
	{
		//std::cerr << "failed i: " << i << ", j: " << j << std::endl;
		//std::cerr << "root: " << root << " path: " << path << std::endl;
		return notroot;
	}

	if (path[i] == '/')
	{
		i++;
	}

	for(; i < path.length(); i++)
	{
		notroot += path[i];
	}

	//std::cerr << "Found NotRoot: " << notroot << std::endl;

	return notroot;
}



int	RsDirUtil::breakupWideDirList(std::wstring path, 
			std::list<std::wstring> &subdirs)
{
	int start = 0;
	unsigned int i;
	for(i = 0; i < path.length(); i++)
	{
		if (path[i] == '/')
		{
			if (i - start > 0)
			{
				subdirs.push_back(path.substr(start, i-start));
			}
			start = i+1;
		}
	}
	// get the final one.
	if (i - start > 0)
	{
		subdirs.push_back(path.substr(start, i-start));
	}
	return 1;
}



bool	RsDirUtil::checkWideDirectory(std::wstring dir)
{
	struct stat buf;
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
	std::string d(dir.begin(), dir.end());
	int val = stat(d.c_str(), &buf);
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
	if (val == -1)
	{
#ifdef RSDIR_DEBUG 
		std::cerr << "RsDirUtil::checkDirectory() ";
		std::cerr << d << " doesn't exist" << std::endl;
#endif
		return false;
	} 
	else if (!S_ISDIR(buf.st_mode))
	{
		// Some other type - error.
#ifdef RSDIR_DEBUG 
		std::cerr << "RsDirUtil::checkDirectory() ";
		std::cerr << d << " is not Directory" << std::endl;
#endif
		return false;
	}
	return true;
}


bool	RsDirUtil::checkWideCreateDirectory(std::wstring dir)
{
	struct stat buf;
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
	std::string d(dir.begin(), dir.end());
	int val = stat(d.c_str(), &buf);
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
	if (val == -1)
	{
		// directory don't exist. create.
/******************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // UNIX
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
		if (-1 == mkdir(d.c_str(), 0777))
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
#else // WIN
		if (-1 == mkdir(d.c_str()))
#endif
/******************************** WINDOWS/UNIX SPECIFIC PART ******************/

		{
#ifdef RSDIR_DEBUG 
		  std::cerr << "check_create_directory() Fatal Error --";
		  std::cerr <<std::endl<< "\tcannot create:" <<d<<std::endl;
#endif
		  return 0;
		}

#ifdef RSDIR_DEBUG 
		std::cerr << "check_create_directory()";
		std::cerr <<std::endl<< "\tcreated:" <<d<<std::endl;
#endif
	} 
	else if (!S_ISDIR(buf.st_mode))
	{
		// Some other type - error.
#ifdef RSDIR_DEBUG 
		std::cerr<<"check_create_directory() Fatal Error --";
		std::cerr<<std::endl<<"\t"<<d<<" is nor Directory"<<std::endl;
#endif
		return 0;
	}
#ifdef RSDIR_DEBUG 
	std::cerr << "check_create_directory()";
	std::cerr <<std::endl<< "\tDir Exists:" <<d<<std::endl;
#endif
	return 1;
}



#include <dirent.h>

bool 	RsDirUtil::cleanupWideDirectory(std::wstring cleandir, std::list<std::wstring> keepFiles)
{

	/* check for the dir existance */
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
	std::string cd(cleandir.begin(), cleandir.end());
	DIR *dir = opendir(cd.c_str());
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
	std::list<std::wstring>::const_iterator it;

	if (!dir)
	{
		return false;
	}

	struct dirent *dent;
	struct stat buf;

	while(NULL != (dent = readdir(dir)))
	{
		/* check entry type */
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
		std::string fname(dent -> d_name);
		std::wstring wfname(fname.begin(), fname.end());
		std::string fullname = cd + "/" + fname;

	 	if (-1 != stat(fullname.c_str(), &buf))
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
		{
			/* only worry about files */
			if (S_ISREG(buf.st_mode))
			{
				/* check if we should keep it */
				if (keepFiles.end() == (it = std::find(keepFiles.begin(), keepFiles.end(), wfname)))
				{
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
					/* can remove */
					remove(fullname.c_str());
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
				}
			}
		}
	}
	/* close directory */
	closedir(dir);

	return true;
}

/* slightly nicer helper function */
bool RsDirUtil::hashWideFile(std::wstring filepath, 
		std::wstring &name, std::string &hash, uint64_t &size)
{
	if (getWideFileHash(filepath, hash, size))
	{
		/* extract file name */
		name = RsDirUtil::getWideTopDir(filepath);
		return true;
	}
	return false;
}


#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

/* Function to hash, and get details of a file */
bool RsDirUtil::getWideFileHash(std::wstring filepath, 
				std::string &hash, uint64_t &size)
{
	FILE *fd;
	int  len;
	SHA_CTX *sha_ctx = new SHA_CTX;
	unsigned char sha_buf[SHA_DIGEST_LENGTH];
	unsigned char gblBuf[512];

	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
	std::string fp(filepath.begin(), filepath.end());

	if (NULL == (fd = fopen64(fp.c_str(), "rb")))
		return false;

	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/

	/* determine size */
 	fseeko64(fd, 0, SEEK_END);
	size = ftello64(fd);
	fseeko64(fd, 0, SEEK_SET);

	SHA1_Init(sha_ctx);
	while((len = fread(gblBuf,1, 512, fd)) > 0)
	{
		SHA1_Update(sha_ctx, gblBuf, len);
	}

	/* reading failed for some reason */
	if (ferror(fd)) 
	{
		delete sha_ctx;
		fclose(fd);
		return false;
	}

	SHA1_Final(&sha_buf[0], sha_ctx);

        std::ostringstream tmpout;
	for(int i = 0; i < SHA_DIGEST_LENGTH; i++)
	{
		tmpout << std::setw(2) << std::setfill('0') << std::hex << (unsigned int) (sha_buf[i]);
	}
	hash = tmpout.str();

	delete sha_ctx;
	fclose(fd);
	return true;
}

bool RsDirUtil::renameWideFile(const std::wstring& from, const std::wstring& to)
{
	int			loops = 0;

#if defined(WIN32) || defined(MINGW) || defined(__CYGWIN__)
#ifdef WIN_CROSS_UBUNTU
	std::wstring f,t ;
	for(int i=0;i<from.size();++i) f.push_back(from[i]) ;
	for(int i=0;i<to.size();++i) t.push_back(to[i]) ;
#else
	std::wstring f(from),t(to) ;
#endif
	while (!MoveFileEx(f.c_str(), t.c_str(), MOVEFILE_REPLACE_EXISTING))
#else
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
        std::string f(from.begin(), from.end());
        std::string t(to.begin(), to.end());
	while (rename(f.c_str(), t.c_str()) < 0)
	/***** XXX TO MAKE WIDE SYSTEM CALL ******************************************************/
#endif
	{
#ifdef WIN32
		if (GetLastError() != ERROR_ACCESS_DENIED)
#else
		if (errno != EACCES)
#endif
			/* set errno? */
			return false ;
#ifdef WIN32
		Sleep(100000);				/* us */
#else
		usleep(100000);				/* us */
#endif

		if (loops >= 30)
			return false ;

		loops++;
	}

	return true ;
}


#endif // WIDE STUFF NOT ENABLED YET!
