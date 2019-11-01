#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/pad.h>
#include <ogc/si.h>
#include <string.h>

//u32 SI_RegisterPollingHandler(RDSTHandler handler);
static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
static u16 *vct = (u16*)0xcc00202c;
static u16 *hct = (u16*)0xcc00202e;
u64 gettick(void);
u64 gettime(void);

static u32 cb_cnt = 0;
static u32 total_clicks;

PADStatus status[4];

static u8 sample[0x100];
static u8 obs[0x100];
static u8 last;
static u32 sidx = 0;

void si_cb()
{
	if (cb_cnt == 0)
	{	
		last = sample[sidx-1];
		memset(obs, 0, sizeof(obs));
		sidx = 0;
	}

	PAD_Read(status);
	if (status[0].button & PAD_BUTTON_A)
	{
		if (sidx > 0)
		{
			if (sample[sidx-1] == 0)
			{
				obs[sidx] = 1;
				total_clicks++;
			}
		}
		else if ((sidx == 0) && (last == 0))
		{
				obs[sidx] = 1;
				total_clicks++;
		}
		sample[sidx] = 1;
	}
	else
	{
		sample[sidx] = 0;
	}

	sidx++;
	cb_cnt++;
}


int main(int argc, char **argv)
{
	u32 res;
	VIDEO_Init();
	PAD_Init();

	// Set the underlying polling rate to 1ms
	SI_SetSamplingRate(1);

	rmode = VIDEO_GetPreferredMode(NULL);

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, 
			rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) 
		VIDEO_WaitVSync();

	printf("mash v0.1\n");
	sleep(1);

	u32 frame = 0;
	cb_cnt = 0;
	SI_RegisterPollingHandler(si_cb);
	u32 hi_perframe = 0;
	u32 perframe = 0;
	while(1) 
	{
		// Wait for the next frame

		VIDEO_WaitVSync();

		// This is the start of a frame
		frame++;

		printf("\x1b[10;0H");
		printf("Current frame: %08d", frame);

		printf("\x1b[11;0H");
		printf("Total clicks:  %08d", total_clicks);

		printf("\x1b[13;0H");
		printf("Samples/frame: %d (samples=%d)", cb_cnt, sidx);
		printf("\x1b[14;0H");
		printf("Sample map:    ");
		for (int i = 0; i < sidx; i++)
			printf("%d ", sample[i]);
		printf("\n");

		printf("\x1b[15;0H");
		for (int j = 0; j < sidx; j++)
			if (obs[j] == 1) perframe++;
		if (perframe > hi_perframe)
			hi_perframe = perframe;
		printf("Max clicks/frame: %d\n", hi_perframe); 
		perframe = 0;

		printf("Latest active sample: ");
		for (int j = 0; j < sidx; j++)
		{
			if (obs[j] == 1)
				printf("%d, ", j);
		}
		cb_cnt = 0;

	}

	return 0;
}
