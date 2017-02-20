#define	LOG2(i)	      (((i) & 0xAAAAAAAA) > ((i) & 0x55555555) ?  1 : 0) \
		    + (((i) & 0xCCCCCCCC) > ((i) & 0x33333333) ?  2 : 0) \
		    + (((i) & 0xF0F0F0F0) > ((i) & 0x0F0F0F0F) ?  4 : 0) \
		    + (((i) & 0xFF00FF00) > ((i) & 0x00FF00FF) ?  8 : 0) \
		    + (((i) & 0xFFFF0000) > 0                  ? 16 : 0)
