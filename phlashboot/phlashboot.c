// ESP32S3 bootloader, started directly after 1st stage (ROM) bootloader
// with a valid stack and UART0 mapped.. the rest is up to us

// Watchdogs we must kick (enabled during boot process, and thus forever)
#define TIM0BASE	0x6001F000
#define WDTCONF_	0x48
#define WDTFEED_	0x60
#define WDTPROT_	0x64
#define TIM0WDTC	(unsigned int *)(TIM0BASE+WDTCONF_)
#define TIM0WDTF	(unsigned int *)(TIM0BASE+WDTFEED_)
#define TIM0WDTP	(unsigned int *)(TIM0BASE+WDTPROT_)

#define RTCBASE		0x60008000
#define RTCWDTCONF_	0x98
#define RTCWDTFEED_	0xAC
#define RTCWDTPROT_	0xB0
#define RTCSWDCONF_	0xB4
#define RTCSWDPROT_	0xB8
#define RTCWDTC		(unsigned int *)(RTCBASE+RTCWDTCONF_)
#define RTCWDTF		(unsigned int *)(RTCBASE+RTCWDTFEED_)
#define RTCWDTP		(unsigned int *)(RTCBASE+RTCWDTPROT_)
#define RTCSWDC		(unsigned int *)(RTCBASE+RTCSWDCONF_)
#define RTCSWDP		(unsigned int *)(RTCBASE+RTCSWDPROT_)

// @see symbol addresses from ROM linker file if esp-idf:
// components/esp_rom/esp32s3/ld/esp32s3.rom.ld, copied to
// our Makefile..
// we call via a pointer to avoid issues with distance to target
extern int Rets_printf(const char *msg, ...);
extern void Rets_delay_us(unsigned int d);

static const int (*ets_printf)(const char *, ...) = Rets_printf;
static const void (*ets_delay_us)(unsigned int) = Rets_delay_us;

// turn off flashboot protection bits in watchdogs, we're an app baby!
static void nukewdts(void) {
	unsigned int *regp;
	// TIMER0 watchdog
	regp = TIM0WDTP;
	*regp = 0x50D83AA1;		// write-protection voodoo
	regp = TIM0WDTC;
	*regp &= ~(1<<14);		// clear flashboot protection bit
	*regp &= ~(1<<31);		// clear wdt enable bit
	regp = TIM0WDTP;
	*regp = 0;
	// RTC watchdog
	regp = RTCWDTP;
	*regp = 0x50D83AA1;		// write-protection voodoo
	regp = RTCWDTC;
	*regp &= ~(1<<12);		// clear flashboot protection bit
	*regp &= ~(1<<31);		// clear wdt enable bit
	regp = RTCWDTP;
	*regp = 0;
	// super watchdog
	regp = RTCSWDP;
	*regp = 0x8F1D312A;		// write-protection voodoo
	regp = RTCSWDC;
	*regp |= (1<<30);		// disable SWD
	regp = RTCSWDP;
	*regp = 0;
}

static char __start_bss;
void entry() {
	// say hi!
	nukewdts();
	ets_printf("Hi Mum!\n");
	// zero .bss
	extern char _end;
	char *_bss = &__start_bss;
	int _bss_len = &_end - _bss;
	for (int b=0; b<_bss_len; b++)
		_bss[b] = 0;
	while(1) {
		ets_printf("Hi Mum (again)!\n");
		ets_delay_us(500000);
	}
}
