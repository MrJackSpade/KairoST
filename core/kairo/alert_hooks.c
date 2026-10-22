/*
 * KairoST: Hatari's alert hooks (ALERT_HOOKS in gui-sdl/dlgAlert.c), which its macOS build uses to
 * show alerts natively. KairoST's machine has no one to click a dialog box: a notice is passed to
 * the host and Hatari carries on, as after its OK button; a question cannot be answered, so it
 * ends the machine with the question as the error.
 *
 * KairoST is free software under the GNU General Public License, version 2 or (at your option)
 * any later version. See LICENSE.md.
 */

#include "main.h"
#include "kairo_st.h"
#include "log.h"

bool HookedAlertNotice(const char *szMessage)
{
	Log_Printf(LOG_WARN, "%s\n", szMessage);
	KairoStHost_Notice(szMessage);
	return true;
}

bool HookedAlertQuery(const char *szMessage)
{
	char message[512];

	snprintf(message, sizeof(message), "Hatari asked a question KairoST cannot answer: %s", szMessage);
	Log_Printf(LOG_ERROR, "%s\n", message);
	KairoStHost_Fatal(message);
	Main_SetQuitValue(1);
	return false;
}
