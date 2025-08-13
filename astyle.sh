#!/bin/bash 


astyle --suffix=none --formatted --options=.astyle --recursive	\
	"main/*.c"			\
       	"components/include/*.c"	\
       	"components/services/*.c"	\
       	"components/dbgTerm/*.c"	\
       	"components/app/*.c"		\
       	"components/cJSON/*.c"		\
					\
	"main/*.h"			\
       	"components/include/*.h"	\
       	"components/services/*.h"	\
       	"components/dbgTerm/*.h"	\
       	"components/app/*.h"		\
       	"components/cJSON/*.h"		\
