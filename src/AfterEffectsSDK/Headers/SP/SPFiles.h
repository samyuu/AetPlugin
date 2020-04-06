/***********************************************************************/
/*                                                                     */
/* SPFiles.h                                                           */
/*                                                                     */
/* Copyright 1995-2006 Adobe Systems Incorporated.                     */
/* All Rights Reserved.                                                */
/*                                                                     */
/* Patents Pending                                                     */
/*                                                                     */
/* NOTICE: All information contained herein is the property of Adobe   */
/* Systems Incorporated. Many of the intellectual and technical        */
/* concepts contained herein are proprietary to Adobe, are protected   */
/* as trade secrets, and are made available only to Adobe licensees    */
/* for their internal use. Any reproduction or dissemination of this   */
/* software is strictly forbidden unless prior written permission is   */
/* obtained from Adobe.                                                */
/*                                                                     */
/***********************************************************************/

#ifndef __SPFiles__
#define __SPFiles__


/*******************************************************************************
 **
 **	Imports
 **
 **/

#include "SPTypes.h"
#include "SPProps.h"

#if defined(IOS_ENV)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(MAC_ENV)
#include <Carbon/Carbon.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 **
 ** Constants
 **
 **/
/** Files suite name */
#define kSPFilesSuite				"SP Files Suite"
/** Files suite version */
#define kSPFilesSuiteVersion		3

/** PICA global list of potential plug-in files. .
	@see \c #SPRuntimeSuite::GetRuntimeFileList().*/
#define kSPRuntimeFileList			((SPFileListRef)NULL)

/** Return value for \c #SPFilesSuite::GetFilePropertyList(),
	indicating that the file has no property list. */
#define kFileDoesNotHavePiPL		(SPPropertyListRef)((intptr_t)-1)
/** Return value for \c #SPFilesSuite::GetFilePropertyList(),
	indicating that the file has multiple property lists. <<is this right? how do you retrieve them?>> */
#define kFileHasMulitplePiPLs		NULL

/*******************************************************************************
 **
 ** Types
 **
 **/

/** The maximum number of characters allowed in a file path specification. */
#define kMaxPathLength 300

/** Opaque reference to a file. Access with the \c #SPFilesSuite. */
typedef struct SPFile *SPFileRef;
/** Opaque reference to a file list. Access with the \c #SPFilesSuite. */
typedef struct SPFileList *SPFileListRef;
/** Opaque reference to a file-list iterator. Access with the \c #SPFilesSuite. */
typedef struct SPFileListIterator *SPFileListIteratorRef;
/** Opaque reference to a platform-specific file specification. Access with the \c #SPFilesSuite. */
typedef struct OpaqueSPPlatformFileRef SPPlatformFileRef;


#if defined(MAC_ENV) || defined(IOS_ENV)
#if PRAGMA_STRUCT_ALIGN		// DRSWAT
#pragma options align=mac68k
#endif

#if defined(IOS_ENV)
	typedef void* PlatformFileRef;
#elif defined(MAC_ENV)
	typedef FSRef PlatformFileRef;
#endif
	
/** A file specification:
		\li In Mac OS, a path string and a FSRef.
		\li In Windows, a path string. */
typedef struct SPPlatformFileSpecification {
	unsigned char path[kMaxPathLength];
	PlatformFileRef mReference;
} SPPlatformFileSpecification;

typedef struct SPPlatformFileSpecificationW {	/* this handles unicode file names */
	PlatformFileRef mReference;
} SPPlatformFileSpecificationW;

#if PRAGMA_STRUCT_ALIGN	// DRSWAT
#pragma options align=reset
#endif

/** Platform-specific file metadata. */
typedef struct SPPlatformFileInfo {	 /* On Mac OS*/
	/** Not used. */
	unsigned int attributes; 	//Unused, but still required to maintain binary compatibility
	/** Date file was created (Mac OS). */
	unsigned int creationDate;
	/** Data file was last modified (Mac OS). */
	unsigned int modificationDate;
	/** Type of file for Finder (Mac OS). */
	unsigned int finderType;
	/** File creator (Mac OS). */
	unsigned int finderCreator;
	/** File flags for Finder; see Mac OS documentation. */
	unsigned short finderFlags;
} SPPlatformFileInfo;
#endif

#if defined(WIN_ENV)  || defined(ANDROID_ENV)
/** A file specification in Windows. */
typedef struct SPPlatformFileSpecification {
	/** The file path string. */
	char path[kMaxPathLength];
} SPPlatformFileSpecification;

/** A widechar file specification in Windows to handle unicode file names. */
typedef struct SPPlatformFileSpecificationW {
	/** mReference could be as long as 64K but MUST be NULL terminated. */
	unsigned short *mReference;
} SPPlatformFileSpecificationW;

/**Platform-specific file metadata. */
typedef struct SPPlatformFileInfo {
	/** File attribute flags; see Windows documentation. */
	unsigned int attributes;
	/** Least-significant byte of the file creation date-time (Windows).*/
	unsigned int lowCreationTime;
	/** Most-significant byte of the file creation date-time (Windows).*/
	unsigned int highCreationTime;
	/** Least-significant byte of the file modification date-time (Windows).*/
	unsigned int lowModificationTime;
	/** Most-significant byte of the file cremodification date-time (Windows).*/
	unsigned int highModificationTime;
	/** The file-name extension indicating the file type (Windows). */
	const char *extension;
} SPPlatformFileInfo;
#endif

/** Internal */
typedef SPBoolean (*SPAddPiPLFilterProc)( SPPlatformFileInfo *info );

#include "SPErrorCodes.h"

#ifdef __cplusplus
}
#endif

#endif
