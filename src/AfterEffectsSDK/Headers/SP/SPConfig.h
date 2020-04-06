/***********************************************************************/
/*                                                                     */
/* SPConfig.h                                                          */
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

/**

	SPConfig.h is the environment configuration file for Sweet Pea. It
	defines MAC_ENV/WIN_ENV/IOS_ENV/ANDROID_ENV. These are used to control platform-specific
	sections of code.

 **/

#ifndef __SPCnfig__
#define __SPCnfig__

#if defined(__APPLE_CC__)
	#include "TargetConditionals.h"
	#if TARGET_OS_MAC
		#if TARGET_OS_IPHONE
		// Older SDKs ( before TV, WATCH ) do not have TARGET_OS_IOS defined.
			#if TARGET_OS_IOS || !defined(TARGET_OS_IOS)
				#ifndef IOS_ENV
					#define IOS_ENV 1
				#endif
			#endif
		#else
			#ifndef MAC_ENV
				#define MAC_ENV 1
			#endif
		#endif // TARGET_OS_IPHONE
	#endif	// TARGET_OS_MAC
#endif

#if defined(__ANDROID__)
    #ifndef ANDROID_ENV
        #define ANDROID_ENV 1
    #endif
#endif

/*
 *	Metrowerks MacOS 68K and PPC
 */
#ifdef __MWERKS__
#if !defined(__INTEL__)		/* mjf was- #if defined(__MC68K__) || defined(__POWERPC__) */
#ifndef MAC_ENV
#define MAC_ENV 1
#endif
#endif
#endif

/*
 *	Metrowerks Windows
 */
#ifdef __MWERKS__
#ifdef __INTEL__
#ifndef WIN_ENV
#define WIN_ENV 1
#include <x86_prefix.h>
#endif
#endif
#endif

/*
 *	Windows
 */
#if defined(_WINDOWS) || defined(_MSC_VER) || defined(WINDOWS)		// PSMod, better compiler check
#ifndef WIN_ENV
#define WIN_ENV 1
#endif
#endif

/*
 *	Make certain that one and only one of the platform constants is defined.
 */

#if !defined(WIN_ENV) && !defined(MAC_ENV) && !defined(IOS_ENV) && !defined(ANDROID_ENV)
	#error
#endif

#if defined(WIN_ENV) && defined(MAC_ENV)
	#error
#endif

#endif
