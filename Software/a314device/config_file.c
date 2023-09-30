#include <exec/types.h>
#include <dos/dos.h>

#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

#include "device.h"

#define CONFIG_FILE_NAME	"DEVS:a314.config"

// Config file parse state
#define PS_KEY_FIRST_CHAR	0
#define PS_KEY			1
#define PS_VALUE_SKIP_WS	2
#define PS_VALUE		3
#define PS_MALFORMED		4

#define SysBase (*(struct ExecBase **)4)

static ULONG str_to_ulong(char *p)
{
	ULONG value = 0;
	char c = *p++;
	BOOL valid = c != 0;
	while (c)
	{
		if (c >= '0' && c <= '9')
			value = (value << 4) | (c - '0');
		else if (c >= 'a' && c <= 'f')
			value = (value << 4) | (c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			value = (value << 4) | (c - 'A' + 10);
		else
		{
			valid = FALSE;
			break;
		}
		c = *p++;
	}
	if (!valid)
		value = (ULONG)-1;
	return value;
}

static void parse_config_file(struct A314Device *dev, char *buf)
{
	char *p = buf;
	char *key_ptr = buf;
	char *value_ptr = NULL;
	char c = *p;
	short state = PS_KEY_FIRST_CHAR;
	while (c)
	{
		if (c == '\n')
		{
			*p = 0;

			if (state == PS_VALUE)
			{
				if (strcmp(key_ptr, "ClockportAddress") == 0)
				{
#if defined(MODEL_CP)
					ULONG value = str_to_ulong(value_ptr);
					if (value != (ULONG)-1)
						dev->clockport_address = value;
#endif
				}
			}

			p++;
			key_ptr = p;
			value_ptr = NULL;

			state = PS_KEY_FIRST_CHAR;
		}
		else if (c == ' ' || c == '\t' || c == '\r')
		{
			if (state == PS_KEY_FIRST_CHAR)
				state = PS_MALFORMED;

			*p = 0;
			p++;
		}
		else if (c == '=')
		{
			if (state == PS_KEY)
				state = PS_VALUE_SKIP_WS;
			else
				state = PS_MALFORMED;

			*p = 0;
			p++;
		}
		else
		{
			if (state == PS_KEY_FIRST_CHAR)
				state = PS_KEY;
			else if (state == PS_VALUE_SKIP_WS)
			{
				value_ptr = p;
				state = PS_VALUE;
			}

			p++;
		}

		c = *p;
	}
}

static char *read_config_file(LONG *length_out)
{
	char *buf = NULL;

	struct DosLibrary *DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 0);
	if (DOSBase)
	{
		BPTR f = Open(CONFIG_FILE_NAME, MODE_OLDFILE);
		if (f)
		{
			LONG res = Seek(f, 0, OFFSET_END);
			LONG length = Seek(f, 0, OFFSET_BEGINNING);
			if (res == 0 && length != -1)
			{
				buf = AllocMem(length + 2, MEMF_ANY);

				LONG actual_length = Read(f, buf, length);
				if (actual_length == length)
				{
					buf[length] = '\n';
					buf[length + 1] = 0;
					*length_out = length;
				}
				else
				{
					FreeMem(buf, length + 2);
					buf = NULL;
				}
			}
			Close(f);
		}
		CloseLibrary((struct Library *)DOSBase);
	}

	return buf;
}

void read_and_parse_config_file(struct A314Device *dev)
{
	LONG length = 0;
	char *buf = read_config_file(&length);
	if (buf)
        {
                parse_config_file(dev, buf);
                FreeMem(buf, length + 2);
        }
}
