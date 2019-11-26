/****************************************************************************
 ************                                                    ************
 ************                    M_TMR_TEST                      ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: kp
 *
 *  Description: Test tool for MDIS drivers implementing TMR profile
 *
 *     Required: libraries: mdis_api, usr_oss, usr_utl
 *     Switches: -
 *
 *---------------------------------------------------------------------------
 * Copyright 1999-2019, MEN Mikro Elektronik GmbH
 ******************************************************************************/
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/m_tmr_drv.h>

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   TYPDEFS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   EXTERNALS                           |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   GLOBALS                             |
+--------------------------------------*/
static int32 G_sigCnt;

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void usage(void);
static void PrintMdisError(char *info);
static void PrintUosError(char *info);
static void TmrShow( MDIS_PATH path );


/********************************* usage ************************************
 *
 *  Description: Print program usage
 *
 *---------------------------------------------------------------------------
 *  Input......: -
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void usage(void)
{
	printf("Usage: m_tmr_test [<opts>] <device> [<opts>]\n");
	printf("Function: Test tool for MDIS drivers implementing TMR profile\n");
	printf("  device       device name..................... [none]    \n");
	printf("Options:\n");
	printf("  -c=<dec>     channel number...................[1]       \n");
	printf("  -o           start for one shot mode......... [no]      \n");
	printf("  -f           start for free running mode..... [no]      \n");
	printf("  -h           halt timer...................... [no]      \n");
	printf("  -p=<dec>     set preload register............ [no]      \n");
	printf("  -s           install signal.................. [no]      \n");
	printf("\n");
	printf("Copyright 1999-2019, MEN Mikro Elektronik GmbH\n%s\n", IdentString);
}

/********************************* SigHandler ********************************
 *
 *  Description: Handle Signals
 *
 *
 *---------------------------------------------------------------------------
 *  Input......: sigCode		signal code
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void __MAPILIB SigHandler( u_int32 sigCode )
{
	G_sigCnt++;
}

/********************************* main *************************************
 *
 *  Description: Program main function
 *
 *---------------------------------------------------------------------------
 *  Input......: argc,argv	argument counter, data ..
 *  Output.....: return	    success (0) or error (1)
 *  Globals....: -
 ****************************************************************************/
int main(int argc, char *argv[])
{
	MDIS_PATH	path=0;
	int32		chan,value,n,startmode=-1,preload=0,instSig=0;
	char		*device,*errstr,buf[40],*str;

	/*--------------------+
    |  check arguments    |
    +--------------------*/
	if ((errstr = UTL_ILLIOPT("c=ofhp=s?", buf))) {	/* check args */
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {						/* help requested ? */
		usage();
		return(1);
	}

	/*--------------------+
    |  get arguments      |
    +--------------------*/
	for (device=NULL, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if (!device) {
		usage();
		return(1);
	}

	G_sigCnt = 0;

	chan 	  = ((str = (UTL_TSTOPT("c="))) ? atoi(str) : 1 );
	startmode = (UTL_TSTOPT("o") ? M_TMR_START_ONE_SHOT : -1);
	startmode = (UTL_TSTOPT("f") ? M_TMR_START_FREE_RUNNING: startmode);
	startmode = (UTL_TSTOPT("h") ? M_TMR_STOP: startmode);
	preload	  = ((str = (UTL_TSTOPT("p="))) ? atoi(str) : 0 );
	instSig   = (UTL_TSTOPT("s") ? 1:0);

	/*--------------------+
    |  open path          |
    +--------------------*/
	if ((path = M_open(device)) < 0) {
		PrintMdisError("open");
		return(1);
	}

	if( UOS_SigInit( SigHandler ) < 0 ){
		PrintUosError("UOS_SigInit");
		goto abort;
	}

	if( UOS_SigInstall( UOS_SIG_USR1 ) < 0 ){
		PrintUosError("UOS_SigInstall");
		goto abort;
	}

	/*--- enable global irqs ---*/
	M_setstat( path, M_MK_IRQ_ENABLE, TRUE );

	/*--- setup current channel ---*/
	if( M_setstat( path, M_MK_CH_CURRENT, chan )){
		PrintMdisError("set current channel");
		goto abort;
	}

	/*--- query profile ---*/
	if( M_getstat( path, M_LL_CH_TYP, &value )){
		PrintMdisError("get channel type");
		goto abort;
	}

	if( value != M_CH_PROFILE_TMR ){
		fprintf(stderr,"Sorry. Channel %d does not implement timer profile\n",
				chan );
		goto abort;
	}


	/*-- query info ---*/
	if( M_getstat( path, M_LL_CH_LEN, &value )){
		PrintMdisError("get channel len");
		goto abort;
	}
	printf("%d bit timer, ", value );

	if( M_getstat( path, M_TMR_RESOLUTION, &value )){
		PrintMdisError("get timer resolution");
		goto abort;
	}
	printf("%d decrements per second.\n", value );

	/*--- show timer state ---*/
	TmrShow(path);

	/*-------------------------------------------+
	|  Perform action specified on command line  |
	+-------------------------------------------*/
	if( instSig ){
		printf("Installing signal\n");
		if( M_setstat( path, M_TMR_SIGSET_ZERO, UOS_SIG_USR1 ) < 0 )
			PrintMdisError("install signal");
	}


	if( preload ){
		printf("Setting preload=%d\n", preload );
		if( M_write( path, preload ) < 0 )
			PrintMdisError("write preload");
	}

	if( startmode != -1){
		printf("Setting start mode=%d\n", startmode );
		if( M_setstat( path, M_TMR_RUN, startmode ) < 0 )
			PrintMdisError("start timer");
	}

	/*-------------+
	|  Show timer  |
	+-------------*/
	do {
		TmrShow(path);
		UOS_Delay(50);
	} while( UOS_KeyPressed() == -1 );

	/*--------------------+
    |  cleanup            |
    +--------------------*/
abort:
	/*--- disable global irqs ---*/
	M_setstat( path, M_MK_IRQ_ENABLE, FALSE );


	if( instSig ){
		if( M_setstat( path, M_TMR_SIGCLR_ZERO, 0 ) < 0 )
			PrintMdisError("remove signal");
	}

	if( UOS_SigRemove( UOS_SIG_USR1 ) < 0 ){
		PrintUosError("UOS_SigInstall");
	}

	if( UOS_SigExit() < 0 ){
		PrintUosError("UOS_SigExit");
	}


	if (M_close(path) < 0)
		PrintMdisError("close");

	return(0);
}


/********************************* TmrShow ***********************************
 *
 *  Description: Show timer (counter) value and state
 *
 *
 *---------------------------------------------------------------------------
 *  Input......: path		path number
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void TmrShow( MDIS_PATH path )
{
	u_int32 value, sigs;
	int32 start;

	if( M_read( path, (int32 *)&value ) < 0 ){
		PrintMdisError("read timer");
		return;
	}

	if( M_getstat( path, M_TMR_RUN, &start ) < 0 ){
		PrintMdisError("get timer state");
		return;
	}

	UOS_SigMask();
	sigs = G_sigCnt;
	G_sigCnt = 0;
	UOS_SigUnMask();

	printf("TMR=0x%08x state=%s %s\n", value,
		   start==M_TMR_STOP ? "stopped" : (start==M_TMR_START_ONE_SHOT ?
											"one shot" : "free running"),
		   sigs ? "*SIG*" : "");

}

/********************************* PrintMdisError ***************************
 *
 *  Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintMdisError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}

/********************************* PrintUosError ****************************
 *
 *  Description: Print UOS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintUosError(char *info)
{
	printf("*** can't %s: %s\n", info, UOS_ErrString(UOS_ErrnoGet()));
}

