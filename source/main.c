#include <stdio.h>
#include <stdlib.h>
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

static u8 sample[0x1000];
static u8 obs[0x1000];
static u32 sidx = 0;
static u8 current_state = 0;

void si_cb()
{
	if (cb_cnt == 0)
	{	
		memset(sample, 0, sizeof(sample));
		memset(obs, 0, sizeof(obs));
		sidx = 0;
	}

	PAD_Read(status);
	if (status[0].button & PAD_BUTTON_A)
	{
		sample[sidx] = 1;
		if (sidx > 0)
		{
			if (sample[sidx-1] == 0)
			{
				total_clicks++;
				obs[sidx] = 1;
			}
		}
	}
	else
	{
		sample[sidx] = 0;
	}

	sidx++;
	cb_cnt++;

	//PAD_ScanPads();
	//u32 pressed = PAD_ButtonsDown(0);
	//if (pressed & PAD_BUTTON_A)
	//	total_clicks++;
	//SI_RegisterPollingHandler(si_cb);
}


int main(int argc, char **argv)
{
	u32 res;
	VIDEO_Init();
	PAD_Init();

	// Set the underlying polling rate to 1ms
	SI_SetSamplingRate(1);
	SI_RegisterPollingHandler(si_cb);

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
	u64 last_frame_time;
	u64 current_frame_time;
	u16 top_vct, top_hct;
	u16 mid_vct, mid_hct;
	u16 bot_vct, bot_hct;
	cb_cnt = 0;
	while(1) 
	{
		top_vct = *vct;
		top_hct = *hct;

		//PAD_ScanPads();
		//u32 pressed = PAD_ButtonsDown(0);
		//if (pressed & PAD_BUTTON_A)
		//	total_clicks++;
		//// We return to the launcher application via exit
		//if ( pressed & PAD_BUTTON_START )
		//	exit(0);

		// Wait for the next frame
		mid_vct = *vct;
		mid_hct = *hct;

		VIDEO_WaitVSync();
		bot_vct = *vct;
		bot_hct = *hct;

		// This is the start of a frame
		frame++;

		printf("\x1b[10;0H");
		printf("frame %08d", frame);

		printf("\x1b[11;0H");
		printf("total clicks %08d", total_clicks);

		printf("\x1b[12;0H");
		current_frame_time = gettime();
		printf("time  %016llx", current_frame_time);

		printf("\x1b[13;0H");
		printf("since %016llx", current_frame_time - last_frame_time);
		last_frame_time = current_frame_time;

		printf("\x1b[14;0H");
		printf("top of loop %04x %04x", top_vct, top_hct);
		printf("\x1b[15;0H");
		printf("before wait %04x %04x", mid_vct, mid_hct);
		printf("\x1b[16;0H");
		printf("bottom loop %04x %04x", bot_vct, bot_hct);
		printf("\x1b[17;0H");
		printf("cb_cnt %d, sidx %d", cb_cnt, sidx);
		printf("\x1b[18;0H");
		for (int i = 0; i < sidx; i++)
			printf("%d ", sample[i]);
		printf("\n");

		printf("observed on sample ");
		for (int j = 0; j < sidx; j++)
		{
			if (obs[j] == 1)
				printf("%d, ", sidx);
		}

		cb_cnt = 0;

	}

	return 0;
}
