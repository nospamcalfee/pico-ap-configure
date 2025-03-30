/*
 home/calfee/Projects/picow_webapp/include/find_local_ssid.h
 */
#ifndef FIND_LOCAL_SSID_H
#define FIND_LOCAL_SSID_H

#include "cdll.h"

extern struct cdll knownnodes;

/*
    return 0 if all ssids have been scanned, or failure code
*/
int scan_find_all_ssids();

/* remove a cdll list included in a my_params struct */
void removelist(struct cdll *p);


#endif //FIND_LOCAL_SSID_H