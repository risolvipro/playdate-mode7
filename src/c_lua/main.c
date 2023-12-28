//
//  main.h
//  PDMode7
//
//  Created by Matteo D'Ignazio on 18/11/23.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"
#include "pd_mode7.h"

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI *playdate, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if(event == kEventInitLua)
    {
        PDMode7_init(playdate, 1);
	}
	
	return 0;
}
