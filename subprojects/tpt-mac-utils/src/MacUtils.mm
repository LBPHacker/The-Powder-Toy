#include "MacUtils.hh"

#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <Foundation/Foundation.h>

void MacUtils_ChdirToDataDirectory()
{
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	NSString *first = paths[0];
	std::string tptPath = [first UTF8String];
	tptPath += "/The Powder Toy";
        mkdir(tptPath.c_str(), 0755);
        chdir(tptPath.c_str());
}
