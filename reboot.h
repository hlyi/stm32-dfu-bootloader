
#ifndef __REBOOT__HH__
#define __REBOOT__HH__

#ifdef USE_BACKUP_REGS

#define RTC_BKP_DR(reg)	(*((volatile uint16_t*) (0x40006C00U + 4 + (4 * (reg)))))
#define PWR_CR		(*((volatile uint32_t*) (0x40007000U)))
#define RCC_APB1ENR	(*((volatile uint32_t*) (0x4002101CU)))

#define PWR_CR_DBP	(1<< 8)
#define RCC_PWR		(1<<28)
#define RCC_BKP		(1<<27)

static void write_backup_register (uint32_t cmd)
{
	RCC_APB1ENR |= RCC_PWR;
	RCC_APB1ENR |= RCC_BKP;

	PWR_CR |= PWR_CR_DBP;

	RTC_BKP_DR(0) = cmd & 0xffff;
	RTC_BKP_DR(1) = (cmd >>16) & 0xffff;
	PWR_CR &= ~PWR_CR_DBP;

}

static uint32_t read_backup_register(void)
{
	return (RTC_BKP_DR(1) << 16 )|RTC_BKP_DR(0);
}

static inline void reboot_into_bootloader() {
	write_backup_register ( 0x544F4F42);
}

static inline void reboot_into_updater() {
	write_backup_register ( 0x53505041 );
}

static inline void clear_reboot_flags() {
	write_backup_register ( 0 );
}

static inline int rebooted_into_dfu() {
	return  0x544F4F42 == read_backup_register() ;
}

static inline int rebooted_into_updater() {
	return  0x53505041 == read_backup_register() ;
}

#else
// Points to the bottom of the stack, we should have 8 bytes free there
extern uint32_t _stack;

// Reboots the system into the bootloader, making sure
// it enters in DFU mode.
static inline void reboot_into_bootloader() {
	uint64_t * ptr = (uint64_t*)&_stack;
	*ptr = 0xDEADBEEFCC00FFEEULL;
}

// Reboots into user app (non-DFU) but doesn't perform any security locks
static inline void reboot_into_updater() {
	uint64_t * ptr = (uint64_t*)&_stack;
	*ptr = 0xDEADBEEF600DF00DULL;
}

// Clears reboot information so we reboot in "normal" mode
static inline void clear_reboot_flags() {
	uint64_t * ptr = (uint64_t*)&_stack;
	*ptr = 0;
}

// Returns whether we were rebooted into DFU mode
static inline int rebooted_into_dfu() {
	uint64_t * ptr = (uint64_t*)&_stack;
	return (*ptr == 0xDEADBEEFCC00FFEEULL);
}

// Returns whether we were rebooted into an updater app
static inline int rebooted_into_updater() {
	uint64_t * ptr = (uint64_t*)&_stack;
	return (*ptr == 0xDEADBEEF600DF00DULL);
}
#endif

#endif


