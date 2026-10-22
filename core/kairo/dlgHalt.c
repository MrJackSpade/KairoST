/*
 * KairoST: Hatari's dlgHalt.c. When the emulated CPU halts after a double bus or address error,
 * Hatari asks whether to reset, debug or quit. KairoST's machine has no one to ask: it ends with
 * the halt as its error, and the player can start the game again.
 *
 * KairoST is free software under the GNU General Public License, version 2 or (at your option)
 * any later version. See LICENSE.md.
 */

#include "main.h"
#include "dialog.h"
#include "kairo_st.h"
#include "log.h"

#define DLGHALT_MSG "Detected double bus/address error => CPU halted!"

void Dialog_HaltDlg(void)
{
	Log_Printf(LOG_ERROR, "%s\n", DLGHALT_MSG);
	KairoStHost_Fatal("The ST's CPU halted after a double bus or address error");
	Main_SetQuitValue(1);
}
