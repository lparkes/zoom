/*
 *  A Z-Machine
 *  Copyright (C) 2000 Andrew Hunter
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Implementation of a metadata parser for the IF Metadata format (version 0.9)
 * Requires expat
 */

#ifndef __IFMETADATA_H
#define __IFMETADATA_H

#include <stdlib.h>

#ifdef HAVE_WCHAR_H
# include <wchar.h>
#endif

#ifdef HAVE_COREFOUNDATION
# include <CoreFoundation/CoreFoundation.h>
#endif

/*
 * Some notes:
 *
 * Not all systems have wchar support, but Zoom in particular supports Unicode anyway,
 * therefore we use 'int' as a general character type (aka UCS-4).
 *
 * expat by default passes characters in UTF-8 format: we convert to UCS-4 internally.
 *
 * #define HAVE_WCHAR_H to enable wchar support
 * #define HAVE_COREFOUNDATION to enable CoreFoundation (OS X) support
 */

/* Data formats */
typedef struct IFMetadata     IFMetadata;
typedef struct IFMDStory      IFMDStory;
typedef struct IFMDIdent      IFMDIdent;
typedef struct IFMDData       IFMDData;
typedef struct IFMDIndexEntry IFMDIndexEntry;

typedef struct IFMDError      IFMDError;

typedef int				      IFMDChar;
typedef unsigned char         IFMDByte;

struct IFMDData {
	IFMDChar* title;
	IFMDChar* headline;
	IFMDChar* author;
	IFMDChar* genre;
	unsigned int year;
	IFMDChar* group;
	enum {
		IFMD_Unrated = 0x0,
		IFMD_Merciful,
		IFMD_Polite,
		IFMD_Tough,
		IFMD_Nasty,
		IFMD_Cruel
	} zarfian;
	IFMDChar* teaser;
	IFMDChar* comment;
	float rating;
};

struct IFMDZCode {
	IFMDByte serial[6];    /* Always has 6 bytes */
	unsigned int release;  /* Only 8 bits used */
	unsigned int checksum; /* Only 16 bits actually used */
};

struct IFMDGlulx {
	IFMDByte serial[6];
	unsigned int release;
};

enum IFMDFormat {
	IFFormat_Unknown = 0x0,
	
	IFFormat_ZCode,
	IFFormat_Glulx,
	
	IFFormat_TADS,
	IFFormat_HUGO,
	IFFormat_Alan,
	IFFormat_Adrift,
	IFFormat_Level9,
	IFFormat_AGT,
	IFFormat_MagScrolls,
	IFFormat_AdvSys
};

struct IFMDIdent {
	enum IFMDFormat format;
	
	enum IFMDFormat dataFormat; /* May be different to format (but only if the XML is poorly written) */
	union {
		struct IFMDZCode zcode;
		struct IFMDGlulx glulx;
	} data;
	
	IFMDByte usesMd5;
	IFMDByte md5Sum[16];
};

struct IFMDStory {
	int         error;
	
	int			numberOfIdents;
	IFMDIdent*  idents;
	
	IFMDData     data;
};

struct IFMDIndexEntry {
	IFMDIdent* ident;
	IFMDStory* story;
};

struct IFMetadata {
	int			    numberOfStories;
	IFMDStory*      stories;
	
	int             numberOfErrors;
	IFMDError*      error;
	
	int             numberOfIndexEntries;
	IFMDIndexEntry* index;
};

/* Errors */
enum IFMDSeverity {
	IFMDErrorWarning = 0x0,
	IFMDErrorFatal
};

enum IFMDErrorType {
	IFMDErrorProgrammerIsASpoon,
	
	IFMDErrorXMLError,
	IFMDErrorNotXML,
	IFMDErrorUnknownVersion,
	IFMDErrorUnknownTag,
	IFMDErrorNotIFIndex,
	IFMDErrorUnknownFormat,
	IFMDErrorMismatchedFormats,
	
	IFMDErrorStoriesShareIDs,
	IFMDErrorDuplicateID
};

struct IFMDError {
	enum IFMDSeverity severity;
	enum IFMDErrorType type;

	char* moreText;
};

/* Functions */

/* Parsing the file */
extern IFMetadata* IFMD_Parse		(const IFMDByte* data, size_t length);
extern void        IFMD_Free		(IFMetadata* oldData);
extern IFMDStory*  IFMD_Find		(IFMetadata* data, const IFMDIdent* id);

/* ID functions */
extern int         IFID_Compare		(const IFMDIdent* a,
									 const IFMDIdent* b);
extern void		   IFID_Free        (IFMDIdent* oldId);

/* Story functions */
extern void        IFStory_Free     (IFMDStory* oldStory);

/* String functions */
extern int		   IFStrLen(const IFMDChar* string);
extern char*	   IFStrnCpyC(char* dst, const IFMDChar* src, size_t sz); /* ASCII */

/* Allocation functions */
extern IFMetadata* IFMD_Alloc(void);
extern IFMDStory*  IFStory_Alloc(void);
extern IFMDIdent*  IFID_Alloc(void);

#ifdef HAVE_WCHAR_H
extern wchar_t*    IFStrnCpyW(wchar_t* dst, const IFMDChar* src, size_t sz); /* UTF-16 */
#endif

#ifdef HAVE_COREFOUNDATION
extern CFStringRef IFStrCpyCF(const IFMDChar* src); /* UTF-16 */
#endif
#endif
