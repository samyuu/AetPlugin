#pragma once

/*  If defined, the following flags inhibit definition
 *     of the indicated items.
*/
// NOTE: CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOGDICAPMASKS
// NOTE: VK_*
#define NOVIRTUALKEYCODES
// NOTE: WM_*, EM_*, LB_*, CB_*
// #define NOWINMESSAGES
// NOTE: WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
// #define NOWINSTYLES
// NOTE: SM_*
#define NOSYSMETRICS
// NOTE: MF_*
#define NOMENUS
// NOTE: IDI_*
#define NOICONS
// NOTE: MK_*
#define NOKEYSTATES
// NOTE: SC_*
// #define NOSYSCOMMANDS
// NOTE: Binary and Tertiary raster ops
#define NORASTEROPS
// NOTE: SW_*
// #define NOSHOWWINDOW
// NOTE: OEM Resource values
#define OEMRESOURCE
// NOTE: Atom Manager routines
#define NOATOM
// NOTE: Clipboard routines
// #define NOCLIPBOARD
// NOTE: Screen colors
#define NOCOLOR
// NOTE: Control and Dialog routines
#define NOCTLMGR
// NOTE: DrawText() and DT_*
#define NODRAWTEXT
// NOTE: All GDI defines and routines
#define NOGDI
// NOTE: All KERNEL defines and routines
#define NOKERNEL
// NOTE: All USER defines and routines
// #define NOUSER
// NOTE: All NLS defines and routines
// #define NONLS
// NOTE: MB_* and MessageBox()
#define NOMB
// NOTE: GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMEMMGR
// NOTE: typedef METAFILEPICT
#define NOMETAFILE
// NOTE: Macros min(a,b) and max(a,b)
#define NOMINMAX
// NOTE: typedef MSG and associated routines
// #define NOMSG
// NOTE: OpenFile(), OemToAnsi, AnsiToOem, and OF_*
// #define NOOPENFILE
// NOTE: SB_* and scrolling routines
#define NOSCROLL
// NOTE: All Service Controller routines, SERVICE_ equates, etc.
#define NOSERVICE
// NOTE: Sound driver routines
#define NOSOUND
// NOTE: typedef TEXTMETRIC and associated routines
#define NOTEXTMETRIC
// NOTE: SetWindowsHook and WH_*
#define NOWH
// NOTE: GWL_*, GCL_*, associated routines
// #define NOWINOFFSETS
// NOTE: COMM driver routines
#define NOCOMM
// NOTE: Kanji support stuff.
#define NOKANJI
// NOTE: Help engine interface.
#define NOHELP
// NOTE: Profiler interface.
#define NOPROFILER
// NOTE: DeferWindowPos routines
// #define NODEFERWINDOWPOS
// NOTE: Modem Configuration Extensions
#define NOMCX
// NOTE: Exclude APIs such as Cryptography, DDE, RPC, Shell, and Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>