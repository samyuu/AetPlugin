/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*
	Entry.h
	
	Part of the Adobe After Effects SDK.
*/

#include "AE_PluginData.h"

#ifdef AE_OS_WIN
	#define DllExport   __declspec( dllexport )
#elif defined AE_OS_MAC
	#define DllExport   __attribute__ ((visibility ("default")))
#endif


#define AE_ENTRY_POINT			 "EffectMain"
#define AE_RESERVED_INFO		 8

#define PF_REGISTER_EFFECT(INPTR, CBPTR, NAME, MATCHNAME, CATEGORY,RESERVEDINFO)  \
       result = (*(CBPTR))((INPTR),\
				reinterpret_cast<const A_u_char*>(NAME),\
				reinterpret_cast<const A_u_char*>(MATCHNAME),\
				reinterpret_cast<const A_u_char*>(CATEGORY),\
				reinterpret_cast<const A_u_char*>(AE_ENTRY_POINT),\
				'eFKT',\
				PF_AE_PLUG_IN_VERSION,\
				PF_AE_PLUG_IN_SUBVERS,\
			    RESERVEDINFO);\
	  if(result == A_Err_NONE)\
	  {\
	     result = PF_Err_NONE;\
	  }
