
// Flashing routines //

#define FLASH_CR_OPTWRE (1 << 9)
#define FLASH_CR_LOCK   (1 << 7)
#define FLASH_CR_STRT   (1 << 6)
#define FLASH_CR_OPTER  (1 << 5)
#define FLASH_CR_OPTPG  (1 << 4)
#define FLASH_CR_PER    (1 << 1)
#define FLASH_CR_PG     (1 << 0)
#define FLASH_SR_BSY    (1 << 0)
#define FLASH_SR_PGERR  (1 << 2)
#define FLASH_SR_WPERR  (1 << 4)
#define FLASH_KEYR    (*(volatile uint32_t*)0x40022004U)
#define FLASH_OPTKEYR (*(volatile uint32_t*)0x40022008U)
#define FLASH_SR      (*(volatile uint32_t*)0x4002200CU)
#define FLASH_CR      (*(volatile uint32_t*)0x40022010U)
#define FLASH_AR      (*(volatile uint32_t*)0x40022014U)

#ifdef ENABLE_CH32F103
#define FLASH_CR_PAGE_PROGRAM	(1<<16)
#define FLASH_CR_PAGE_ERASE	(1<<17)
#define FLASH_CR_BUF_LOAD	(1<<18)
#define FLASH_CR_BUF_RST	(1<<19)
#define FLASH_MODEKEYP (*(volatile uint32_t*)0x40022024U)
#define FLASH_PGADDR (*(volatile uint32_t*)0x40022034U)
#endif

static void _flash_lock() {
	// Clear the unlock state.
	FLASH_CR |= FLASH_CR_LOCK;
}

static void _flash_unlock() {
	// Only if locked!
	if (FLASH_CR & FLASH_CR_LOCK) {
		// Authorize the FPEC access.
		FLASH_KEYR = 0x45670123U;
		FLASH_KEYR = 0xcdef89abU;
#ifdef ENABLE_CH32F103
		FLASH_MODEKEYP = 0x45670123U;
		FLASH_MODEKEYP = 0xcdef89abU;
#endif
	}
}

#define _flash_wait_for_last_operation() \
	/* 1 cycle wait, see STM32 errata */ \
	do {                                 \
		__asm__ volatile("nop");         \
	} while (FLASH_SR & FLASH_SR_BSY);

static void _flash_erase_page(uint32_t page_address) {
	_flash_wait_for_last_operation();

	FLASH_CR |= FLASH_CR_PER;
	FLASH_AR = page_address;
	FLASH_CR |= FLASH_CR_STRT;

	_flash_wait_for_last_operation();

	FLASH_CR &= ~FLASH_CR_PER;
}

static int _flash_page_is_erased(uint32_t addr) {
	volatile uint32_t *_ptr32 = (uint32_t*)addr;
	for (unsigned i = 0; i < 1024/sizeof(uint32_t); i++)
		if (_ptr32[i] != 0xffffffffU)
			return 0;
	return 1;
}

static void _flash_program_buffer(uint32_t address, uint16_t *data, unsigned len) {
	_flash_wait_for_last_operation();

#ifdef ENABLE_CH32F103
	uint32_t * dst_ptr = (uint32_t *) address;
	uint32_t * src_ptr = (uint32_t *) data;
	uint32_t last_word = ((len+3) >> 2)-1;			// assume word aligned
	if ( (address & 0x7f )!= 0 ) return;			// address need 128-Byte aligned
	for ( uint32_t i = 0; i <= last_word; i ++ ) {
		// at page boundary
		if ( ( i & 0x1f) == 0 ) {
			// program page
			FLASH_CR |= FLASH_CR_PAGE_PROGRAM;
			FLASH_CR |= FLASH_CR_BUF_RST;			// reset page buffer
			_flash_wait_for_last_operation();
			FLASH_CR &= ~FLASH_CR_PAGE_PROGRAM;
		}
		if ( ( i & 3 ) == 0 ){
			FLASH_CR |= FLASH_CR_PAGE_PROGRAM;
		}
		*dst_ptr = *src_ptr++;
		if ( ( (i & 3 ) == 3) || (i == last_word) ) {
			uint32_t pg_adr = ((uint32_t)dst_ptr) & (~0x0fUL);

			FLASH_CR |= FLASH_CR_BUF_LOAD;			// load page buffer
			_flash_wait_for_last_operation();
			FLASH_CR &= ~FLASH_CR_PAGE_PROGRAM;
			FLASH_PGADDR = *(volatile uint32_t*)(pg_adr ^ 0x00000100);    // taken from example
			if ( ( (i&0x1f) == 0x1f) || (i==last_word) ){
				pg_adr = ((uint32_t)dst_ptr) & (~0x7f);
				FLASH_CR |= FLASH_CR_PAGE_PROGRAM;
				FLASH_AR = pg_adr;
				FLASH_CR |= FLASH_CR_STRT;
				_flash_wait_for_last_operation();
				FLASH_CR &= ~FLASH_CR_PAGE_PROGRAM;
				FLASH_PGADDR = *(volatile uint32_t*)(pg_adr^ 0x00000100);    // taken from example
			}
		}
		dst_ptr++;
	}
#else
	// Enable programming
	FLASH_CR |= FLASH_CR_PG;

	volatile uint16_t *addr_ptr = (uint16_t*)address;
	for (unsigned i = 0; i < len/2; i++) {
		addr_ptr[i] = data[i];
		_flash_wait_for_last_operation();
	}

	// Disable programming
	FLASH_CR &= ~FLASH_CR_PG;
#endif
}

#if defined(ENABLE_PROTECTIONS) || defined(ENABLE_WRITEPROT)
static void _flash_erase_option_bytes() {
	_flash_wait_for_last_operation();

	FLASH_CR |= FLASH_CR_OPTER;
	FLASH_CR |= FLASH_CR_STRT;

	_flash_wait_for_last_operation();

	FLASH_CR &= ~FLASH_CR_OPTER;
}

static void _flash_program_option_bytes(uint32_t address, uint16_t data) {
	_flash_wait_for_last_operation();

	FLASH_CR |= FLASH_CR_OPTPG;  // Enable option byte programming.
	volatile uint16_t *addr_ptr = (uint16_t*)address;
	*addr_ptr = data;
	_flash_wait_for_last_operation();
	FLASH_CR &= ~FLASH_CR_OPTPG;  // Disable option byte programming.
}

static void _optbytes_unlock() {
	if (!(FLASH_CR & FLASH_CR_OPTWRE)) {
		// F1 uses same keys for flash and option
		FLASH_OPTKEYR = 0x45670123U;
		FLASH_OPTKEYR = 0xcdef89abU;
	}
}
#endif

#ifdef ENABLE_SAFEWRITE
static void check_do_erase() {
	// For protection reasons, we do not allow reading the flash using DFU
	// and also we make sure to wipe the entire flash on an ERASE/WRITE command
	// just to guarantee that nobody is able to extract the data by flashing a
	// stub and executing it.

	static int erased = 0;
	if (erased) return;

	/* Change usb_strings accordingly */
	const uint32_t start_addr = FLASH_BASE_ADDR + (FLASH_BOOTLDR_SIZE_KB*1024);
	const uint32_t end_addr   = FLASH_BASE_ADDR + (        FLASH_SIZE_KB*1024);
	for (uint32_t addr = start_addr; addr < end_addr; addr += 1024)
		if (!_flash_page_is_erased(addr))
			_flash_erase_page(addr);

	erased = 1;
}
#endif


