// cup_info.c

#include <unistd.h>
#include <sys/sysctl.h>

#include "cpu_info.h"

long 
configured_processors ( void ) 
{
    return ( long ) sysconf( ( int ) _SC_NPROCESSORS_CONF );
}

long
online_processors ( void ) 
{
    return ( long ) sysconf( ( int ) _SC_NPROCESSORS_ONLN );
}
