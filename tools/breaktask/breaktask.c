/* breaktask NAME: sends Ctrl-C to the task or process called NAME - for a
 * program started from its icon, which AmiAgent's STOP (CLI processes only)
 * cannot reach. Build: m68k-amigaos-gcc -O1 -noixemul -o breaktask breaktask.c */
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dos.h>

int main(int argc, char **argv)
{
	struct Task *t;
	if (argc < 2) {
		PutStr("breaktask NAME\n");
		return 10;
	}
	Forbid();
	t = FindTask((CONST_STRPTR)argv[1]);
	if (t != NULL) Signal(t, SIGBREAKF_CTRL_C);
	Permit();
	PutStr(t != NULL ? "Ctrl-C sent\n" : "no such task\n");
	return t != NULL ? 0 : 5;
}
