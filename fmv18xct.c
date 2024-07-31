/**
		FMV-18x Configuration Tool
		2024 mcDomDom
*/
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Configuration registers only on the '865A/B chips. */
#define EEPROM_Ctrl 	16
#define EEPROM_Data 	17
#define CARDSTATUS		16			/* FMV-18x Card Status */
#define CARDSTATUS1		17			/* FMV-18x Card Status */
#define IOCONFIG		18		/* Either read the jumper, or move the I/O. */
#define IOCONFIG1		19
#define	SAPROM			20		/* The station address PROM, if no EEPROM. */
#define RESET			31		/* Write to reset some parts of the chip. */

/* Model identification.  */
#define FE_FMV0_MODEL			0x07
#define FE_FMV0_MODEL_FMV181	0x05	/* FMV-181/181A		*/
#define FE_FMV0_MODEL_FMV182	0x03	/* FMV-182/182A/184	*/
#define FE_FMV0_MODEL_FMV183	0x04	/* FMV-183		*/

/* Card type ID */
#define FE_FMV1_MAGIC_MASK	0xB0
#define FE_FMV1_MAGIC_VALUE	0x00
#define FE_FMV1_CARDID_REV	0x0F
#define FE_FMV1_CARDID_REV_A	0x01	/* FMV-181A/182A	*/
#define FE_FMV1_CARDID_PNP	0x08	/* FMV-183/184		*/

#define outb outp
#define inb inp
#define	u_short	unsigned short
#define	u_char	unsigned char

static int fmv18x_probe_list[] = {
	0x220, 0x240, 0x260, 0x280, 0x2a0, 0x2c0, 0x300, 0x340, 0
};

static char fmv181_irqmap[4] = {3, 7, 10, 15};	// FMV-181/182
static char fmv183_irqmap[4] = {4, 5, 10, 11};	// FMV-183/184


// É{Å[ÉhÇÃâûìöë“Çø
void fmv_wait(int ioaddr)
{
	while ((inb(ioaddr+CARDSTATUS) & 0x40) != 0x40);
}

void fmv_eeprom_start(int ioaddr)
{
	outb(ioaddr+0x1B, 0x46);
	outb(ioaddr+0x1E, 0x04);
	outb(ioaddr+0x1F, 0xC8);
	outb(ioaddr+0x1E, 0x06);
}

void fmv_eeprom_end(int ioaddr)
{
	outb(ioaddr+0x1E, 0x05);
	outb(ioaddr+0x1F, 0x40);
}

int parse_argv(int argc, char *argv[], int pnp, u_char *irq_io, u_char *pnp_mode)
{
	int i, j, irq, ioaddr, irq_idx, ioaddr_idx, ret = 0;
	
	irq_idx = ioaddr_idx = -1;
	for (i=1; i<argc; i++) {
		if (strnicmp(argv[i], "-IRQ=", 5) == 0) {
			irq = atoi(&argv[i][5]);
			for (j=0; j<4; j++) {
				if ((pnp && fmv183_irqmap[j] == irq) || 
					(!pnp && fmv181_irqmap[j] == irq)) {
					irq_idx = j;
				}
			}
			if (0 <= irq_idx) {
				*irq_io &= 0x3F;	// clear 7,6bit
				*irq_io |= (irq_idx << 6);
				ret |= 1;
			}
			else {
				printf("IRQ(%d) is invalid\n", irq);
				ret = -1;
				break;
			}
		}
		else if (strnicmp(argv[i], "-IO=", 4) == 0) {
			ioaddr = strtol(&argv[i][4], NULL, 16);
			for (j=0; j<8; j++) {
				if (fmv18x_probe_list[j] == ioaddr) {
					ioaddr_idx = j;
				}
			}
			if (0 <= ioaddr_idx) {
				*irq_io &= 0xF8;	// clear 2-0bit
				*irq_io |= ioaddr_idx;
				ret |= 1;
			}
			else {
				printf("I/O(%03X) is invalid\n", ioaddr);
				ret = -1;
				break;
			}
		}
		else if (stricmp(argv[i], "-ON") == 0) {
			*pnp_mode |= 0x01;
			ret |= 2;
		}
		else if (stricmp(argv[i], "-OFF") == 0) {
			*pnp_mode &= 0xFE;
			ret |= 2;
		}
	}
	
	return ret;
}

int main(int argc, char *argv[])
{
	char fmv_irqmap_pnp[8] = {3, 4, 5, 7, 9, 10, 11, 15};
	char szModel[10];
	u_char irq_io, pnp_mode;

	int i, j, irq, ioaddr, found=0, model, id, type, ret, pnp=0;
	
	ret = 0;
	
	printf("FMV-181/182/183/184 Configuration Tool\n");
	printf("2024 mcDomDom\n");
	for (i=0; fmv18x_probe_list[i] !=0 ; i++) {
		ioaddr = fmv18x_probe_list[i];
		if (inb(ioaddr   + SAPROM    ) == 0x00
			&& inb(ioaddr + SAPROM + 1) == 0x00
			&& inb(ioaddr + SAPROM + 2) == 0x0e) {

			/* Determine the card type. */
			model = inb(ioaddr+CARDSTATUS) & FE_FMV0_MODEL;
			id    = inb(ioaddr+CARDSTATUS1) & FE_FMV1_CARDID_REV;
			
			switch (model) {
			case FE_FMV0_MODEL_FMV181:
				strcpy(szModel, "FMV-181");
				if (id == FE_FMV1_CARDID_REV_A)
					strcpy(szModel, "FMV-181A");
				break;
			case FE_FMV0_MODEL_FMV182:
				strcpy(szModel, "FMV182");
				if (id == FE_FMV1_CARDID_REV_A)
					strcpy(szModel, "FMV-182A");
				else if (id == FE_FMV1_CARDID_PNP) {
					strcpy(szModel, "FMV-184");
					pnp = 1;
				}
				break;
			case FE_FMV0_MODEL_FMV183:
				strcpy(szModel, "FMV-183");
				pnp = 1;
				break;
			default:
				type = 0;
				strcpy(szModel, "Unknown");
				break;
			}

			printf("%s found I/O:%04X (model:%02X id:%02X)\n", szModel, ioaddr, model, id);
			printf("Get MAC");
			for (j=0; j<6; j++) printf(":%02X", inb(ioaddr+SAPROM+j));
			printf("\n");
			if (inb(ioaddr + CARDSTATUS1) & 0x20) {
				printf("PnP Mode Enabled\n");
			}
			printf("Get I/O:%04X\n", fmv18x_probe_list[inb(ioaddr + IOCONFIG) & 0x07]);
			printf("Get IRQ:% 2d\n", pnp ? fmv183_irqmap[(inb(ioaddr + IOCONFIG)>>6) & 0x03]
										 : fmv181_irqmap[(inb(ioaddr + IOCONFIG)>>6) & 0x03]);
			printf("Get Media:");
			switch (inb(ioaddr + CARDSTATUS)) {
			case 0x01:
				printf("10base5\n");
				break;
			case 0x02:
				printf("10base2\n");
				break;
			case 0x04:
				printf("10baseT\n");
				break;
			default:
				printf("auto-sense\n");
				break;
			}
			found = 1;
			break;
		}
	}
	if (found == 0) {
		printf("not found FMV-18x\n");
	}
	else {
		/*
		for (i=0; i<16; i++) {
			printf("%02X:%02X ", 0x10+i, inp(ioaddr+0x10+i));
		}
		printf("\n");
		*/

		fmv_eeprom_start(ioaddr);
		
		fmv_wait(ioaddr);
		irq_io = inb(ioaddr+0x12);
		printf("EEPROM(0x00):%02X\n", irq_io);
		fmv_wait(ioaddr);
		pnp_mode = inb(ioaddr+0x1F);
		printf("EEPROM(0x02):%02X\n", pnp_mode);
		fmv_wait(ioaddr);
		
		ret = parse_argv(argc, argv, pnp, &irq_io, &pnp_mode);
		if (0 < ret) {
			if (ret & 1) {
				fmv_wait(ioaddr);
				printf("EEPROM(0x00) Write [%02X]\n", irq_io);
				outb(ioaddr+0x12, irq_io);
				fmv_wait(ioaddr);
			}
			if (pnp && ret & 2) {
				fmv_wait(ioaddr);
				printf("EEPROM(0x02) Write [%02X]\n", pnp_mode);
				outb(ioaddr+0x1F, pnp_mode);
				fmv_wait(ioaddr);
			}
			
			printf("Please reset your PC\n");
		}
		
		fmv_eeprom_end(ioaddr);
	}
	printf("\n");
	
	if (argc <= 1 || ret == -1) {
		printf("usage: %s (-IRQ=x) (-IO=0xXXX) (-ON/-OFF)\n", argv[0]);
		printf("FMV-181/182 IRQ:3/7/10/15\n");
		printf("FMV-183/184 IRQ:4/5/10/11\n");
		printf("I/O:");
		for (i=0; i<8; i++) {
			printf("0x%03X", fmv18x_probe_list[i]);
			if (i<8-1) printf("/");
		}
		printf("\n");
		printf("-ON is PnP ON\n");
		printf("-OFF is PnP OFF\n");
	}
	
	return 0;
}
