/* shader utilities */

#include <stdlib.h>
#include <stdio.h>

#include "shutil.h"

char *shader_fread(char *name)
{
	char *buffer;
	long fsize;
	FILE *fh;

	buffer = NULL;
	fh = fopen(name, "r");

	if (fh) {
		fseek(fh, 0L, SEEK_END);
		fsize = ftell(fh);
		rewind(fh);

		buffer = (char *) calloc(1, fsize);

		if (buffer) {
			fread(buffer, fsize, 1, fh);
		}

		fclose(fh);
	} else {
		perror("can't open shader file");
	}

	return buffer;
}
