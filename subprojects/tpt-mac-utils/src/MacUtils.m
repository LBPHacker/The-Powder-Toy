#include "MacUtils.h"

#include <string.h>
#include <Foundation/Foundation.h>

char *MacUtils_GetDataDirectory()
{
	const char *theirs = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES)[0] UTF8String];
	char *ours = malloc(strlen(theirs) + 1);
	strcpy(ours, theirs);
	return ours;
}
