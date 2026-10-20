/* reboot - restart the Amiga at once (exec ColdReboot). For test runs that
 * leave the machine with a damaged memory list; started through AmiAgent. */
#include <proto/exec.h>
#include <proto/dos.h>

int main(void)
{
	Delay(25);   /* let AmiAgent answer the RUN/START first */
	ColdReboot();
	return 0;
}
