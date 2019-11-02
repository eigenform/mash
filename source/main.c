#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/pad.h>
#include <ogc/si.h>
#include <string.h>
#include <unistd.h>

#define ticks_to_us(ticks) ((((u64)(ticks)*8)/(u64)(TB_TIMER_CLOCK/125)))
#define ticks_to_ms(ticks)    (((u64)(ticks)/(u64)(TB_TIMER_CLOCK)))

u32 gettick(void);
u64 gettime(void);

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
void init()
{
	VIDEO_Init();
	PAD_Init();
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
}

/* This is basically all of our state.
 */


static u32 frame_ctr;			// The current frame number
static u32 total_clicks;		// The total number of clicks
static bool recording = false;		// Are we currently inside a window?

static u32 window_start_ts;		// Timestamp of window start in ticks
static u32 last_window_start_ts;	// Timestamp of previous window start
static float last_window_time_us;	// Length of previous window in us

static u32 downtime_ts;
static float last_downtime_us;

static bool test_req = false;

struct sample
{
	u32 cb_ts;	// Ticks when callback was fired
	u32 read_ts;	// Ticks immediately after PADRead()
	u32 pressed;	// Was the 'A' button pressed? (1=yes)
};

float clicks[2000];
u32 click_idx = 0;


struct sample last_sample;		// The previous sample
struct sample cur_sample;		// The current sample

u32 press_start_ts;			// Ticks at the rising edge
float last_press_time_us;
float best_press_time_us = 1000000;

PADStatus status[4];			// Array of per-controller input data
void si_cb()
{
	// If we're currently recording data
	if (recording)
	{
		cur_sample.cb_ts = gettick();
		PAD_Read(status);
		cur_sample.read_ts = gettick();

		cur_sample.pressed = (status[0].button & PAD_BUTTON_A) ? 1:0;

		// Is this the rising edge of an 'A' press?
		if ((last_sample.pressed == 0) && (cur_sample.pressed == 1))
		{
			press_start_ts = gettick();
			total_clicks++;
		}

		// Is this the falling edge of an 'A' press?
		if ((last_sample.pressed == 1) && (cur_sample.pressed == 0))
		{
			last_press_time_us = ticks_to_us(gettick() - press_start_ts);
			if (last_press_time_us < best_press_time_us)
				best_press_time_us = last_press_time_us;
			clicks[click_idx] = last_press_time_us;
			click_idx++;

		}

		last_sample = cur_sample;
	}
	// Otherwise, we're handling other inputs
	else
	{
		PAD_Read(status);
		test_req = (status[0].button & PAD_BUTTON_START) ? true:false;
	}
}

void write_stats()
{
	printf("\x1b[10;0H");
	printf("Current frame:       %08d", frame_ctr);

	printf("\x1b[11;0H");
	printf("                                        ");
	printf("\x1b[11;0H");
	printf("Last frame took:     %.3fms", last_window_time_us/1000);

	printf("\x1b[13;0H");
	printf("                                        ");
	printf("\x1b[13;0H");
	printf("Last click took:     %.3fms", last_press_time_us/1000);

	printf("\x1b[14;0H");
	printf("                                        ");
	printf("\x1b[14;0H");
	printf("Quickest click took: %.3fms", best_press_time_us/1000);

	printf("\x1b[17;0H");
	printf("Total clicks:        %08d", total_clicks);


	printf("\n");
}


// Mark the beginning of a sampling window.
void start_window()
{
	window_start_ts = gettick();
	recording = true;
	frame_ctr++;

	last_window_time_us = ticks_to_us(window_start_ts - last_window_start_ts);
	last_downtime_us = ticks_to_us(window_start_ts - downtime_ts);
	last_window_start_ts = window_start_ts;
}

// Mark the end of a sampling window.
void stop_window()
{
	recording = false;

	// Start recording how much downtime until the start of a new window
	downtime_ts = gettick();
}

void do_countdown()
{
	u32 start_ts;
	u32 target_ms = 10000;
	u32 current_ms = 0;


	printf("Counting down from 10 ...\n");

	VIDEO_WaitVSync();
	start_ts = gettick();
	float cur_s;
	while (1)
	{
		current_ms = ticks_to_ms(gettick() - start_ts);
		cur_s = target_ms - current_ms;

		printf("\x1b[9;0H");
		printf("                                        ");
		printf("\x1b[9;0H");
		printf("          Starting in %.2f ....", cur_s/1000);

		if (current_ms >= target_ms)
			break;
		VIDEO_WaitVSync();
	}

	// Clear screen, then cursor to 0;0
	printf("\x1b[2J");
	printf("\x1b[0;0H");
}

void do_test()
{
	u32 start_ts;
	// By serializing the start of the mainloop with vsync, we can ensure 
	// that the first window will be aligned to the boundary of a frame.

	VIDEO_WaitVSync();

	start_ts = gettick();
	while(1) 
	{
		// Here, the polling handler will sample controller input
		// until we arrive at the next vertical blanking period.
		// With the sampling rate at 1ms, this means 16 samples.

		start_window();
		VIDEO_WaitVSync();

		// At the start of a frame, take some time to draw data.
		// The array of samples is locked until a new window starts.
		// The execution time of these MUST NOT exceed 1000us.
		// Otherwise, we risk missing the next sample.

		stop_window();
		write_stats();
		if (ticks_to_ms(gettick() - start_ts) >= 10000)
			break;
	}

	float click_length_avg_us = 0;
	for (int i = 0; i < click_idx; i++)
	{
		click_length_avg_us += clicks[i];
	}
	click_length_avg_us = click_length_avg_us / total_clicks;
	printf("Avg click took:      %.3fms\n", click_length_avg_us/1000);

	printf("Press START to [re]start sampling for 10 seconds.\n");
}


void cleanup()
{
	click_idx = 0;
	frame_ctr = 0;
	total_clicks = 0;
	best_press_time_us = 1000000;
	memset(clicks, 0, sizeof(clicks));
}


// Main loop
int main(int argc, char **argv)
{
	init();

	// Set the underlying polling rate to 1ms.
	// I have absolutely no idea what behaviour is on PAL.

	SI_SetSamplingRate(1);

	printf("mash v0.1\n");
	printf("Press START to [re]start sampling for 10 seconds.\n");
	sleep(1);


	SI_RegisterPollingHandler(si_cb);
	VIDEO_WaitVSync();

	while(1) 
	{
		// Start a test
		if (test_req)
		{
			cleanup();
			printf("\x1b[2J");
			printf("\x1b[0;0H");
			printf("Starting test...\n");
			// Countdown from 10
			do_countdown();
			do_test();
		}
		VIDEO_WaitVSync();
	}

	return 0;
}
