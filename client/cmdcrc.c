//-----------------------------------------------------------------------------
// Copyright (C) 2015 iceman <iceman at iuse.se>
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// CRC Calculations from the software reveng commands
//-----------------------------------------------------------------------------

#include <stdlib.h>
#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#  ifndef STDIN_FILENO
#    define STDIN_FILENO 0
#  endif /* STDIN_FILENO */
#endif /* _WIN32 */

#include <stdio.h>
#include <string.h>
//#include <stdlib.h>
//#include <ctype.h>
#include "cmdmain.h"
#include "cmdcrc.h"
#include "reveng/reveng.h"
#include "ui.h"
#include "util.h"

#define MAX_ARGS 20

int uerr(char *msg){
	PrintAndLog("%s",msg);
	return 0;
}

int split(char *str, char *arr[MAX_ARGS]){
	int beginIndex = 0;
	int endIndex;
	int maxWords = MAX_ARGS;
	int wordCnt = 0;

	while(1){
		while(isspace(str[beginIndex])){
			++beginIndex;
		}
		if(str[beginIndex] == '\0')
			break;
			endIndex = beginIndex;
		while (str[endIndex] && !isspace(str[endIndex])){
			++endIndex;
		}
		int len = endIndex - beginIndex;
		char *tmp = calloc(len + 1, sizeof(char));
		memcpy(tmp, &str[beginIndex], len);
		arr[wordCnt++] = tmp;
		//PrintAndLog("cnt: %d, %s",wordCnt-1, arr[wordCnt-1]);
		beginIndex = endIndex;
		if (wordCnt == maxWords)
			break;
	}
	return wordCnt;
}

int CmdCrc(const char *Cmd)
{
	char name[] = {"reveng "};
	char Cmd2[50 + 7];
	memcpy(Cmd2, name, 7);
	memcpy(Cmd2 + 7, Cmd, 50);
	char *argv[MAX_ARGS];
	int argc = split(Cmd2, argv);
	//PrintAndLog("argc: %d, %s %s Cmd: %s",argc, argv[0], Cmd2, Cmd);
	reveng_main(argc, argv);
	for(int i = 0; i < argc; ++i){
		//puts(arr[i]);
		free(argv[i]);
	}

	return 0;
}

int GetModels(char *Models[], int *count, uint32_t *width){
	/* default values */
	static model_t model = {
		PZERO,		/* no CRC polynomial, user must specify */
		PZERO,		/* Init = 0 */
		P_BE,		/* RefIn = false, RefOut = false, plus P_RTJUST setting in reveng.h */
		PZERO,		/* XorOut = 0 */
		PZERO,		/* check value unused */
		NULL		/* no model name */
	};
	int ibperhx = 8;//, obperhx = 8;
	int rflags = 0, uflags = 0; /* search and UI flags */

	//unsigned long width = 0UL;
	//int c, mode = 0, args, psets, pass;
	poly_t apoly, crc, qpoly = PZERO, *apolys = NULL, *pptr = NULL, *qptr = NULL;
	model_t pset = model, *candmods, *mptr;
	//char *string;

	//myname = argv[0];

	/* stdin must be binary */
	#ifdef _WIN32
		_setmode(STDIN_FILENO, _O_BINARY);
	#endif /* _WIN32 */

	SETBMP();
	
	//pos=0;
	//optind=1;

	int args = 0, psets, pass;
	int Cnt = 0;
	if (*width == 0) { //reveng -D
		*count = mcount();
		//PrintAndLog("Count: %d",*count);
		if(!*count)
			return uerr("no preset models available");

		for(int mode = 0; mode < *count; ++mode) {
			mbynum(&model, mode);
			mcanon(&model);
			size_t size = (model.name && *model.name) ? strlen(model.name) : 6;
			//PrintAndLog("Size: %d, %s",size,model.name);
			char *tmp = calloc(size+1, sizeof(char));
			if (tmp==NULL)
				return uerr("out of memory?");

			memcpy(tmp, model.name, size);
			Models[mode] = tmp;
			//ufound(&model);
		}
	} else { //reveng -s

			if(~model.flags & P_MULXN)
				return uerr("cannot search for non-Williams compliant models");

			praloc(&model.spoly, *width);
			praloc(&model.init, *width);
			praloc(&model.xorout, *width);
			if(!plen(model.spoly))
				palloc(&model.spoly, *width);
			else
				*width = plen(model.spoly);

			/* special case if qpoly is zero, search to end of range */
			if(!ptst(qpoly))
				rflags &= ~R_HAVEQ;


			/* not going to be sending additional args

			// allocate argument array 
			args = argc - optind;
			if(!(apolys = malloc(args * sizeof(poly_t))))
				return uerr("cannot allocate memory for argument list");

			for(pptr = apolys; optind < argc; ++optind) {
				if(uflags & C_INFILE)
					*pptr++ = rdpoly(argv[optind], model.flags, ibperhx);
				else
					*pptr++ = strtop(argv[optind], model.flags, ibperhx);
			}
			// exit value of pptr is used hereafter! 
		
			*/

			/* if endianness not specified, try
			 * little-endian then big-endian.
			 * NB: crossed-endian algorithms will not be
			 * searched.
			 */
			/* scan against preset models */
			if(~uflags & C_FORCE) {
				pass = 0;
				Cnt = 0;
				do {
					psets = mcount();
					//PrintAndLog("psets: %d",psets);
					while(psets) {
						mbynum(&pset, --psets);
						
						/* skip if different width, or refin or refout don't match */
						if(plen(pset.spoly) != *width || (model.flags ^ pset.flags) & (P_REFIN | P_REFOUT))
							continue;
						/* skip if the preset doesn't match specified parameters */
						if(rflags & R_HAVEP && pcmp(&model.spoly, &pset.spoly))
							continue;
						if(rflags & R_HAVEI && psncmp(&model.init, &pset.init))
							continue;
						if(rflags & R_HAVEX && psncmp(&model.xorout, &pset.xorout))
							continue;
				
						apoly = pclone(pset.xorout);
						if(pset.flags & P_REFOUT)
							prev(&apoly);
						for(qptr = apolys; qptr < pptr; ++qptr) {
							crc = pcrc(*qptr, pset.spoly, pset.init, apoly, 0);
							if(ptst(crc)) {
								pfree(&crc);
								break;
							} else
								pfree(&crc);
						}
						pfree(&apoly);
						if(qptr == pptr) {
							/* the selected model solved all arguments */
							mcanon(&pset);
							
							size_t size = (pset.name && *pset.name) ? strlen(pset.name) : 6;
							//PrintAndLog("Size: %d, %s, count: %d",size,pset.name, Cnt);
							char *tmp = calloc(size+1, sizeof(char));
							if (tmp==NULL){
								PrintAndLog("out of memory?");
								return 0;
							}
							memcpy(tmp, pset.name, size);
							Models[Cnt++] = tmp;
							*count = Cnt;
							//ufound(&pset);
							uflags |= C_RESULT;
						}
					}
					mfree(&pset);

					/* toggle refIn/refOut and reflect arguments */
					if(~rflags & R_HAVERI) {
						model.flags ^= P_REFIN | P_REFOUT;
						for(qptr = apolys; qptr < pptr; ++qptr)
							prevch(qptr, ibperhx);
					}
				} while(~rflags & R_HAVERI && ++pass < 2);
			}
			if(uflags & C_RESULT) {
				for(qptr = apolys; qptr < pptr; ++qptr)
					pfree(qptr);
				return 1;
				//exit(EXIT_SUCCESS);
			}
			if(!(model.flags & P_REFIN) != !(model.flags & P_REFOUT))
				return uerr("cannot search for crossed-endian models");
			pass = 0;
			do {
				mptr = candmods = reveng(&model, qpoly, rflags, args, apolys);
				if(mptr && plen(mptr->spoly))
					uflags |= C_RESULT;
				while(mptr && plen(mptr->spoly)) {
					/* results were printed by the callback
					 * string = mtostr(mptr);
					 * puts(string);
					 * free(string);
					 */
					mfree(mptr++);
				}
				free(candmods);
				if(~rflags & R_HAVERI) {
					model.flags ^= P_REFIN | P_REFOUT;
					for(qptr = apolys; qptr < pptr; ++qptr)
						prevch(qptr, ibperhx);
				}
			} while(~rflags & R_HAVERI && ++pass < 2);
			for(qptr = apolys; qptr < pptr; ++qptr)
				pfree(qptr);
			free(apolys);
			if(~uflags & C_RESULT)
				return uerr("no models found");


	}
	//PrintAndLog("DONE");
	return 1;
}

//test call to GetModels
int CmdrevengTest(const char *Cmd){
	char *Models[80];
	int count = 0;
	uint32_t width = 0;
	width = param_get8(Cmd, 0);
	//PrintAndLog("width: %d",width);
	if (width > 89)
		return uerr("Width cannot exceed 89");

	int ans = GetModels(Models, &count, &width);
	if (!ans) return 0;
	
	PrintAndLog("Count: %d",count);
	for (int i = 0; i < count; i++){
		PrintAndLog("Model %d: %s",i,Models[i]);
		free(Models[i]);
	}
	return 1;
}
