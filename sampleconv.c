// converts samples between text and binary formats
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef int16_t i16;

void error (const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	fprintf (stderr, "sampleconv: ");
	vfprintf (stderr, fmt, ap);
	fputc ('\n', stderr);
	va_end (ap);
	exit (1);
}

// READ FUNCTIONS

typedef bool(*read_t)(FILE *, i16 *);

bool read_float (FILE *file, i16 *x)
{
	char line[32], *endp;
	double d;
	long i;

	if (fgets (line, sizeof (line), file) == NULL)
		return false;

	d = strtod (line, &endp);
	if (*endp != '\0' && *endp != '\n')
		error ("invalid float: %s", line);

	i = (long)(d * 32767);
	if (i < -32768 || i > 32767)
		error ("out of range: %s", line);

	*x = (i16)i;
	return true;
}

bool read_int (FILE *file, i16 *x)
{
	char line[32], *endp;
	long i;

	if (fgets (line, sizeof (line), file) == NULL)
		return false;

	i = strtol (line, &endp, 10);
	if (*endp != '\0' && *endp != '\n')
		error ("invalid int: %s", line);

	if (i < -32768 || i > 32767)
		error ("out of range: %s", line);

	*x = (i16)i;
	return true;
}

bool read_bin (FILE *file, i16 *x)
{
	char b[2];
	if (fread (b, 2, 1, file) != 2)
		return false;
	*x = (i16)b[0] | ((i16)b[1] << 8);
	return true;
}

// WRiTE FUNCTIONS

typedef void(*write_t)(FILE *, i16);

void write_float (FILE *file, i16 x)
{
	fprintf (file, "%f\n", (float)x / 32767);
}

void write_int (FILE *file, i16 x)
{
	fprintf (file, "%d\n", (int)x);
}

void write_bin (FILE *file, i16 x)
{
	char b[2];
	b[0] = x & 0xff;
	b[1] = x >> 8;
	fwrite (b, 2, 1, file);
}

int usage (void)
{
	fputs ("usage: sampleconv [-i filetype] [-o filetype]\n", stderr);
	return 1;
}

int main (int argc, char *argv[])
{
	read_t rfunc = read_float;
	write_t wfunc = write_bin;
	int option;
	i16 x;

	while ((option = getopt (argc, argv, "i:o:")) != -1) {
		switch (option) {
		case 'i':
			if (strcmp (optarg, "float") == 0) {
				rfunc = read_float;
			} else if (strcmp (optarg, "int") == 0) {
				rfunc = read_int;
			} else if (strcmp (optarg, "bin") == 0) {
				rfunc = read_bin;
			} else {
				error ("invalid input type: %s", optarg);
			}
			break;
		case 'o':
			if (strcmp (optarg, "float") == 0) {
				wfunc = write_float;
			} else if (strcmp (optarg, "int") == 0) {
				wfunc = write_int;
			} else if (strcmp (optarg, "bin") == 0) {
				wfunc = write_bin;
			} else {
				error ("invalid input type: %s", optarg);
			}
			break;
		default:
			return usage ();
		}
	}

	while (rfunc (stdin, &x))
		wfunc (stdout, x);


	return 0;
}
