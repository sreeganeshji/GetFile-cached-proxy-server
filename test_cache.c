
#include <stdio.h>
#include "simplecache.h"
#include <sys/stat.h>
#include<sys/stat.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<fcntl.h>

unsigned long int cache_delay;

int main()
{
 cache_delay = 0;

	simplecache_init("locals.txt");
int fd = simplecache_get("/courses/ud923/filecorpus/yellowstone.jpg");

//int fd = simplecache_get("yellowstone.jpg");
struct stat filestat;

int fstatStatus = fstat(fd,&filestat);


simplecache_destroy();
	return -1;
}

