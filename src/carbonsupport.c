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
 * Some useful support functions for Mac OS X (Carbon)
 */

#include "../config.h"

#if WINDOW_SYSTEM == 3

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <Carbon/Carbon.h>

#include "zmachine.h"
#include "file.h"
#include "display.h"
#include "zoomres.h"
#include "rc.h"
#include "hash.h"
#include "xfont.h"
#include "carbondisplay.h"

enum zcode_type
  {
    TYPE_ZCOD,
    TYPE_IFZS,
    TYPE_BORING
  };

NavDialogRef thedialog;
static FSRef* fileref = NULL;
static FSRef* startref = NULL;
static int    opengame_finished = 0;
FSRef* lastopenfs = NULL;
FSRef* forceopenfs = NULL;

/* Display a message box */
void carbon_display_message(char* title, char* message)
{
  if (window_available == 0)
    {
      Str255 tit, erm;
      SInt16 item;

      AlertStdAlertParamRec par;
      
      par.movable = false;
      par.helpButton = false;
      par.filterProc = nil;
      par.defaultText = "\010Continue";
      par.cancelText = nil;
      par.otherText = nil;
      par.defaultButton = kAlertStdAlertOKButton;
      par.cancelButton = 0;
      par.position = 0;

      strcpy(tit+1, title);
      strcpy(erm+1, message);
      tit[0] = strlen(title);
      erm[0] = strlen(message);
      
      StandardAlert(kAlertNoteAlert, tit, erm, &par, &item);
    }
  else
    {
      AlertStdCFStringAlertParamRec par;
      OSStatus res;
      CFStringRef tit = nil;
      CFStringRef erm = nil;
      DialogRef msgdlog;

      tit = CFStringCreateWithCString(NULL, title, kCFStringEncodingMacRoman);
      erm = CFStringCreateWithCString(NULL, message, kCFStringEncodingMacRoman);
      
      par.version       = kStdCFStringAlertVersionOne;
      par.movable       = false;
      par.helpButton    = false;
      par.defaultText   = CFSTR("Continue");
      par.cancelText    = nil;
      par.otherText     = nil;
      par.defaultButton = kAlertStdAlertOKButton;
      par.cancelButton  = 0;
      par.position      = kWindowDefaultPosition;
      par.flags         = 0;
      
      res = CreateStandardSheet(kAlertNoteAlert, 
				tit,
				erm,
				&par,
				GetWindowEventTarget(zoomWindow),
				&msgdlog);

      if (res == noErr)
	res = ShowSheetWindow(GetDialogWindow(msgdlog), zoomWindow);

      if (res != noErr)
	{
	  Str255 tit, erm;
	  SInt16 item;

	  strcpy(tit+1, title);
	  strcpy(erm+1, message);
	  tit[0] = strlen(title);
	  erm[0] = strlen(message);

	  StandardAlert(kAlertNoteAlert, tit, erm, NULL, &item);
	}
    }
}

/* A utility function for getting the type of a file */
static enum zcode_type type_fsref(FSRef* file)
{
  FSSpec spec;
  HFSUniStr255 outname;
  
  FInfo inf;

  /* Get the name of this item (and the FSSpec...) */
  FSGetCatalogInfo(file, kFSCatInfoNone, NULL,
		   &outname, &spec, NULL);

  /* Get the finder info for this item */
  FSpGetFInfo(&spec, &inf);

  switch (inf.fdType)
    {
    case 'ZCOD':
      return TYPE_ZCOD;

    case 'IFZS':
      return TYPE_IFZS;

    case '????':
    case 'TEXT':
    case 'BINA':
    case 0:
    default:
      if (outname.unicode[outname.length-3] == '.' &&
	  outname.unicode[outname.length-2] == 'z' &&
	  outname.unicode[outname.length-1] >= '3' &&
	  outname.unicode[outname.length-1] <= '8')
	return TYPE_ZCOD;
      if (outname.unicode[outname.length-4] == '.' &&
	  outname.unicode[outname.length-3] == 'q' &&
	  outname.unicode[outname.length-2] == 'u' &&
	  outname.unicode[outname.length-1] == 't')
	return TYPE_IFZS;
      break;
    }

  return TYPE_BORING;
}

/* Some handlers for some of the standard events */
OSErr ae_open_handler(const AppleEvent* evt,
		      AppleEvent* reply,
		      SInt32      handlerRefIcon)
{
  mac_openflag = 1;

  return noErr;
}

OSErr ae_reopen_handler(const AppleEvent* evt,
			AppleEvent* reply,
			SInt32      handlerRefIcon)
{
  return noErr;
}

OSErr ae_quit_handler(const AppleEvent* evt,
		      AppleEvent* reply,
		      SInt32      handlerRefIcon)
{
  display_exit(1);

  return noErr;
}

OSErr ae_print_handler(const AppleEvent* evt,
		       AppleEvent* reply,
		       SInt32      handlerRefIcon)
{
  carbon_display_message("Zoom " VERSION " - error",
			 "You cannot print ZCode files");

  return noErr;
}

OSErr ae_opendocs_handler(const AppleEvent* evt,
			  AppleEvent*       reply,
			  SInt32            handlerRefIcon)
{
  AEDescList docs;
  OSErr      erm;
  SInt32     numdocs;

  FSRef           thefile;
  enum zcode_type thetype;

  AEKeyword key;
  DescType  actualtype;
  Size      actualsize;

  /* Get the FSRef of the relevant documents */
  erm = AEGetParamDesc(evt, keyDirectObject, typeAEList, &docs);
  
  if (erm != noErr)
    return erm;

  AECountItems(&docs, &numdocs);

  if (numdocs <= 0)
    {
      return noErr;
    }

  if (numdocs > 1)
    {
      /* Display a warning */
      carbon_display_message("Zoom " VERSION " - note", "Zoom can only handle one game file (or save game file) at once - only the first of the specified files will be opened");
    }

  AEGetNthPtr(&docs, 1, typeFSRef, &key, &actualtype, &thefile, 
	      sizeof(FSRef), &actualsize);

  /* 
   * Work out the type of the file that's being opened
   */
  thetype = type_fsref(&thefile);
 
  /*
   * Might happen if the application is running (in which case we might try
   * to restore a savegame?), or if it's not, in which case we have a
   * file to return from carbon_get_zcode_file
   */
  if (mac_openflag == 1)
    {
      /*
       * Zoom is running... are we looking for a file to run?
       */
      if (opengame_finished != 0)
	{
	  if (thetype != TYPE_IFZS)
	    {
	      /* Display a warning */
	      carbon_display_message("Zoom " VERSION " - note",
				     "Zoom cannot load a new game while one is being played: quit and restart Zoom to start a new game");
	    }
	}
    }

  /* Try to open the file (it now being OK to do so...) */
  switch (thetype)
    {
    case TYPE_ZCOD:
      if (mac_openflag == 0)
	{
	  startref = malloc(sizeof(FSRef));
	  *startref = thefile;
	}
      else
	{
	  fileref = malloc(sizeof(FSRef));
	  *fileref = thefile;
	  opengame_finished = 1;
	}
      break;

    case TYPE_IFZS:
      if (window_available == 0)
	{
	  carbon_display_message("Zoom " VERSION " - note",
				 "Zoom cannot currently locate a game from its save file alone - you will need to run the game before restoring the save file");
	}
      else
	{
	  display_force_restore(&thefile);
	  if (!display_force_input("restore"))
	    {
	      carbon_display_message("Zoom " VERSION " - note",
				     "Zoom is not currently in a state where it can force a restore");
	    }
	}
      break;

    case TYPE_BORING:
      break; /* Well, we're broken, at any rate */
    }

  mac_openflag = 1;
  return noErr;
}

/* Filters out all except Z-Code files */
static Boolean zcode_filter(AEDesc*        theItem,
			    void*          info,
			    void*          ioUserData,
			    NavFilterModes filterMode)
{
  Boolean              display = true;
  NavFileOrFolderInfo  *theInfo;

  theInfo = (NavFileOrFolderInfo *) info;
  /* Hmm, I have a code snippet that uses typeFSS here - hmpfl */
  if(theItem->descriptorType == typeFSRef)
    {
      if(!theInfo->isFolder)
	{
	  switch (theInfo->fileAndFolder.fileInfo.finderInfo.fdType)
	    {
	    case 'ZCOD':
	      display = true;
	      break;

	    case '????':
	    case 'TEXT':
	    case 0:
	      {
		HFSUniStr255 outName;
		FSRef ourref;
		
		/* Hooray, yet another incorrectly documented function */
		AEGetDescData(theItem,
			      &ourref, sizeof(FSRef));

		/* Get the name of this item */
		FSGetCatalogInfo(&ourref, kFSCatInfoNone,
				 NULL,
				 &outName,
				 NULL,
				 NULL);

		/* See if the extension is .z[345678]*/
		if (outName.unicode[outName.length-3] == '.' &&
		    outName.unicode[outName.length-2] == 'z' &&
		    outName.unicode[outName.length-1] >= '3' &&
		    outName.unicode[outName.length-1] <= '8')
		  {
		    display = true;
		  }
		else
		  {
		    display = false;
		  }
	      }
	      break;
	      
	    default:
	      display = false;
	    }
	}
    }

  return display;
}

/* Filters out all except savegame files */
Boolean savegame_filter(AEDesc*        theItem,
			void*          info,
			void*          ioUserData,
			NavFilterModes filterMode)
{
  Boolean              display = true;
  NavFileOrFolderInfo  *theInfo;

  theInfo = (NavFileOrFolderInfo *) info;
  /* Hmm, I have a code snippet that uses typeFSS here - hmpfl */
  if(theItem->descriptorType == typeFSRef)
    {
      if(!theInfo->isFolder)
	{
	  switch (theInfo->fileAndFolder.fileInfo.finderInfo.fdType)
	    {
	    case 'IFZS':
	      display = true;
	      break;

	    case '????':
	    case 'TEXT':
	    case 0:
	      {
		HFSUniStr255 outName;
		FSRef ourref;
		
		/* Hooray, yet another incorrectly documented function */
		AEGetDescData(theItem,
			      &ourref, sizeof(FSRef));

		/* Get the name of this item */
		FSGetCatalogInfo(&ourref, kFSCatInfoNone,
				 NULL,
				 &outName,
				 NULL,
				 NULL);

		/* See if the extension is .qut */
		if (outName.unicode[outName.length-4] == '.' &&
		    outName.unicode[outName.length-3] == 'q' &&
		    outName.unicode[outName.length-2] == 'u' &&
		    outName.unicode[outName.length-1] == 't')
		  {
		    display = true;
		  }
		else
		  {
		    display = false;
		  }
	      }
	      break;
	      
	    default:
	      display = false;
	    }
	}
    }

  return display;
}

/* Handler for the 'new game' dialog box */
void nav_evt_handler(NavEventCallbackMessage select,
		     NavCBRecPtr             parm,
		     NavCallBackUserData     data)
{
  switch (select)
    {
    case kNavCBUserAction:
      {
	NavReplyRecord reply;
	NavUserAction  act;

	NavDialogGetReply(parm->context, &reply);
	act = NavDialogGetUserAction(parm->context);

	switch (act)
	  {
	  case kNavUserActionCancel:
	    opengame_finished = 1;
	    break;

	  case kNavUserActionOpen:
	    {
	      SInt32 count;

	      opengame_finished = 1;

	      AECountItems(&reply.selection, &count);
	      if (count >= 1)
		{
		  AEKeyword kw;
		  DescType  outtype;
		  Size      outsize;

		  /* 
		   * We're only interested in the first file, if
		   * multiple files are selected 
		   */
		  if (fileref == NULL)
		    fileref = malloc(sizeof(FSRef));

		  AEGetNthPtr(&reply.selection, 1, typeFSRef,
			      &kw, &outtype, fileref, sizeof(FSRef), &outsize);

		  if (outtype != typeFSRef)
		    zmachine_fatal("Bad item type (internal error)");
		}
	    }
	    break;
	  }

	NavDisposeReply(&reply);
      }
      break;
    }
}

/* 
 * Gets the ZCode file that we're going to play
 * (Carbon version of menu.c, basically)
 */
FSRef* carbon_get_zcode_file(void)
{
  NavDialogCreationOptions  dlOpts;
  static NavEventUPP        nvUPP = NULL;
  static NavObjectFilterUPP flUPP = NULL;
  EventTargetRef target;

  if (fileref != NULL)
    free(fileref);

  if (startref != NULL)
    {
      lastopenfs = fileref = startref;
      startref = NULL;
      opengame_finished = 1;

      return fileref;
    }

  fileref = NULL;

  /* Create the UPP if necessary */
  if (nvUPP == NULL)
    nvUPP = NewNavEventUPP(nav_evt_handler);
  if (flUPP == NULL)
    flUPP = NewNavObjectFilterUPP(zcode_filter);

  /* Get the default options */
  NavGetDefaultDialogCreationOptions(&dlOpts);

  /*
   * NOTE: There is a subtle bug in Navigation services when application
   * modal dialogs are used: Apple Events break, in that the first event
   * you receive will produce a noOutstandingHLE error, and any future
   * events will all be one event behind. This creates a fairly bizarre
   * experience for the user, to say the least.
   */
  dlOpts.modality     = kWindowModalityNone;

  /* Create the dialog box */
  NavCreateGetFileDialog(&dlOpts, nil, 
			 nvUPP,
			 NULL,
			 flUPP,
			 NULL,
			 &thedialog);
  
  /* Run the dialog */
  opengame_finished = 0;
  NavDialogRun(thedialog);

  target = GetEventDispatcherTarget();

  while (!quitflag && !opengame_finished)
    {
      EventRef event;

      if (ReceiveNextEvent(0, NULL, kEventDurationForever, true, &event) == noErr)
	{
	  SendEventToEventTarget(event, target);
	  ReleaseEvent(event);
	}
    }

  /* Dispose of the dialog */
  NavDialogDispose(thedialog);

  lastopenfs = fileref;
  return fileref;
}

/* Functions for dealing with savegames */

/* Event handler for the savegame dialog */
static int savegame_finished = 0;

void save_evt_handler(NavEventCallbackMessage select,
		      NavCBRecPtr             parm,
		      NavCallBackUserData     data)
{
  switch (select)
    {
    case kNavCBUserAction:
      {
	NavReplyRecord reply;
	NavUserAction  act;

	NavDialogGetReply(parm->context, &reply);
	act = NavDialogGetUserAction(parm->context);

	switch (act)
	  {
	  case kNavUserActionCancel:
	    savegame_finished = 1;
	    break;

	  case kNavUserActionOpen:
	    {
	      SInt32 count;

	      savegame_finished = 1;

	      AECountItems(&reply.selection, &count);
	      if (count >= 1)
		{
		  AEKeyword kw;
		  DescType  outtype;
		  Size      outsize;

		  /* 
		   * We're only interested in the first file, if
		   * multiple files are selected 
		   */
		  if (fileref == NULL)
		    fileref = malloc(sizeof(FSRef));

		  AEGetNthPtr(&reply.selection, 1, typeFSRef,
			      &kw, &outtype, fileref, sizeof(FSRef), &outsize);

		  if (outtype != typeFSRef)
		    zmachine_fatal("Bad item type (internal error)");
		}
	    }
	    break;

	  case kNavUserActionSaveAs:
	    {
	      AEDesc       aeDesc;
	      UniCharCount len;
	      UniChar*     filename;
	      FSRef        parent;
	      OSStatus     erm;

	      AECoerceDesc(&reply.selection, typeFSRef, &aeDesc);
	      AEGetDescData(&aeDesc, &parent, sizeof(FSRef));
	      
	      /* Get the filename */
	      len = CFStringGetLength(reply.saveFileName);
	      filename = NewPtr(len);
	      CFStringGetCharacters(reply.saveFileName, CFRangeMake(0,len),
				    filename);

	      if (filename != NULL)
		{
		  FSSpec spec;
		  FInfo  inf;

		  if (reply.replacing)
		    {
		      /* If we're replacing, delete the old file */
		      FSRef oldfile;
		      
		      erm = FSMakeFSRefUnicode(&parent, len, filename,
					       kTextEncodingUnicodeDefault,
					       &oldfile);
		      if (erm == noErr)
			erm = FSDeleteObject(&oldfile);
		      if (erm != noErr)
			break;
		    }

		  if (fileref == NULL)
		    fileref = malloc(sizeof(FSRef));

		  FSCreateFileUnicode(&parent, len, filename, kFSCatInfoNone,
				      NULL, fileref, &spec);
		  
		  FSpGetFInfo(&spec, &inf);

		  inf.fdType    = 'IFZS';
		  inf.fdCreator = SIGNATURE;

		  FSpSetFInfo(&spec, &inf);

		  savegame_finished = 1;
		}
	    }
	    break;
	  }

	NavDisposeReply(&reply);
      }
      break;
    }
}

ZFile* get_file_write(int* fsize)
{
  NavDialogCreationOptions  dlOpts;
  static NavEventUPP        nvUPP = NULL;
  EventTargetRef target;

  if (fileref != NULL)
    free(fileref);

  fileref = NULL;

  /* Create the UPP if necessary */
  if (nvUPP == NULL)
    nvUPP = NewNavEventUPP(save_evt_handler);

  /* Get the default options */
  NavGetDefaultDialogCreationOptions(&dlOpts);

  dlOpts.parentWindow = zoomWindow;
  dlOpts.modality     = kWindowModalityWindowModal;

  /* Create the dialog box */
  NavCreatePutFileDialog(&dlOpts,
			 'IFZS',
			 SIGNATURE,
			 nvUPP,
			 NULL,
			 &thedialog);
  savegame_finished = 0;

  /* Run the dialog */
  NavDialogRun(thedialog);

  target = GetEventDispatcherTarget();

  /* Handle events (clunk, clunk) */
  while (!quitflag && !savegame_finished)
    {
      EventRef event;

      if (ReceiveNextEvent(0, NULL, kEventDurationForever, true, &event) == noErr)
	{
	  SendEventToEventTarget(event, target);
	  ReleaseEvent(event);
	}
    }

  /* Dispose of the dialog */
  NavDialogDispose(thedialog);

  lastopenfs = fileref;

  if (fileref != NULL)
    {
      if (fsize != NULL)
	(*fsize) = 0;
      return open_file_write_fsref(fileref);
    }

  return NULL;
}

ZFile* get_file_read(int* fsize)
{
  NavDialogCreationOptions  dlOpts;
  static NavEventUPP        nvUPP = NULL;
  static NavObjectFilterUPP flUPP = NULL;
  EventTargetRef target;

  if (fileref != NULL)
    free(fileref);

  fileref = NULL;

  if (forceopenfs != NULL)
    {
      fileref = lastopenfs = forceopenfs;
      forceopenfs = NULL;

      if (fsize != NULL)
	*fsize = get_file_size_fsref(fileref);

      return open_file_fsref(fileref);
    }

  /* Create the UPP if necessary */
  if (nvUPP == NULL)
    nvUPP = NewNavEventUPP(save_evt_handler);
  if (flUPP == NULL)
    flUPP = NewNavObjectFilterUPP(savegame_filter);

  /* Get the default options */
  NavGetDefaultDialogCreationOptions(&dlOpts);

  dlOpts.parentWindow = zoomWindow;
  dlOpts.modality     = kWindowModalityWindowModal;

  /* Create the dialog box */
  NavCreateGetFileDialog(&dlOpts, nil, 
			 nvUPP,
			 NULL,
			 flUPP,
			 NULL,
			 &thedialog);
  savegame_finished = 0;

  /* Run the dialog */
  NavDialogRun(thedialog);

  target = GetEventDispatcherTarget();

  /* Handle events (clunk, clunk) */
  while (!quitflag && !savegame_finished)
    {
      EventRef event;

      if (ReceiveNextEvent(0, NULL, kEventDurationForever, true, &event) == noErr)
	{
	  SendEventToEventTarget(event, target);
	  ReleaseEvent(event);
	}
    }

  /* Dispose of the dialog */
  NavDialogDispose(thedialog);

  lastopenfs = fileref;

  if (fileref != NULL)
    {
      if (fsize != NULL)
	(*fsize) = get_file_size_fsref(fileref);
      return open_file_fsref(fileref);
    }

  return NULL;
}

void display_force_restore(FSRef* file)
{
  if (forceopenfs == NULL)
    forceopenfs = malloc(sizeof(FSRef));
  *forceopenfs = *file;
}

#endif

