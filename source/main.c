#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

//u32 gettick(void)
//{
//	u32 result;
//	__asm__ __volatile__ (
//		"mftb	%0\n"
//		: "=r" (result)
//	);
//	return result;
//}
//
//
//u64 gettime(void)
//{
//	u32 tmp;
//	union uulc {
//		u64 ull;
//		u32 ul[2];
//	} v;
//
//	__asm__ __volatile__(
//		"1:	mftbu	%0\n\
//		    mftb	%1\n\
//		    mftbu	%2\n\
//			cmpw	%0,%2\n\
//			bne		1b\n"
//		: "=r" (v.ul[0]), "=r" (v.ul[1]), "=&r" (tmp)
//	);
//	return v.ull;
//}

u64 gettime(void);

int main(int argc, char **argv)
{
	// Initialise the video system
	VIDEO_Init();

	PAD_Init();

	// Set the underlying polling rate to 1ms
	SI_SetSamplingRate(1);

	rmode = VIDEO_GetPreferredMode(NULL);

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	printf("mash v0.1\n");
	u32 frame = 0;
	u32 clicks = 0;
	u64 time;
	while(1) 
	{
		printf("\x1b[2;0H");
		printf("frame %08d", frame);
		printf("\x1b[3;0H");
		printf("click %08d", clicks);


		PAD_ScanPads();
		u32 pressed = PAD_ButtonsDown(0);
		if (pressed & PAD_BUTTON_A)
			clicks++;

		// We return to the launcher application via exit
		if ( pressed & PAD_BUTTON_START )
			exit(0);

		// Wait for the next frame
		VIDEO_WaitVSync();
		frame++;

		printf("\x1b[4;0H");
		time = gettime();
		printf("time  %016llx", time);
	}

	return 0;
}
