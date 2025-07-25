/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  S390 version
 *    Copyright IBM Corp. 1999, 2000
 *    Author(s): Hartmut Penner (hp@de.ibm.com)
 *               Ulrich Weigand (weigand@de.ibm.com)
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/pgtable.h"
 */

#ifndef _ASM_S390_PGTABLE_H
#define _ASM_S390_PGTABLE_H

#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/cpufeature.h>
#include <linux/page-flags.h>
#include <linux/radix-tree.h>
#include <linux/atomic.h>
#include <asm/ctlreg.h>
#include <asm/bug.h>
#include <asm/page.h>
#include <asm/uv.h>

extern pgd_t swapper_pg_dir[];
extern pgd_t invalid_pg_dir[];
extern void paging_init(void);
extern struct ctlreg s390_invalid_asce;

enum {
	PG_DIRECT_MAP_4K = 0,
	PG_DIRECT_MAP_1M,
	PG_DIRECT_MAP_2G,
	PG_DIRECT_MAP_MAX
};

extern atomic_long_t direct_pages_count[PG_DIRECT_MAP_MAX];

static inline void update_page_count(int level, long count)
{
	if (IS_ENABLED(CONFIG_PROC_FS))
		atomic_long_add(count, &direct_pages_count[level]);
}

/*
 * The S390 doesn't have any external MMU info: the kernel page
 * tables contain all the necessary information.
 */
#define update_mmu_cache(vma, address, ptep)     do { } while (0)
#define update_mmu_cache_range(vmf, vma, addr, ptep, nr) do { } while (0)
#define update_mmu_cache_pmd(vma, address, ptep) do { } while (0)

/*
 * ZERO_PAGE is a global shared page that is always zero; used
 * for zero-mapped memory areas etc..
 */

extern unsigned long empty_zero_page;
extern unsigned long zero_page_mask;

#define ZERO_PAGE(vaddr) \
	(virt_to_page((void *)(empty_zero_page + \
	 (((unsigned long)(vaddr)) &zero_page_mask))))
#define __HAVE_COLOR_ZERO_PAGE

/* TODO: s390 cannot support io_remap_pfn_range... */

#define pte_ERROR(e) \
	pr_err("%s:%d: bad pte %016lx.\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	pr_err("%s:%d: bad pmd %016lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pud_ERROR(e) \
	pr_err("%s:%d: bad pud %016lx.\n", __FILE__, __LINE__, pud_val(e))
#define p4d_ERROR(e) \
	pr_err("%s:%d: bad p4d %016lx.\n", __FILE__, __LINE__, p4d_val(e))
#define pgd_ERROR(e) \
	pr_err("%s:%d: bad pgd %016lx.\n", __FILE__, __LINE__, pgd_val(e))

/*
 * The vmalloc and module area will always be on the topmost area of the
 * kernel mapping. 512GB are reserved for vmalloc by default.
 * At the top of the vmalloc area a 2GB area is reserved where modules
 * will reside. That makes sure that inter module branches always
 * happen without trampolines and in addition the placement within a
 * 2GB frame is branch prediction unit friendly.
 */
extern unsigned long VMALLOC_START;
extern unsigned long VMALLOC_END;
#define VMALLOC_DEFAULT_SIZE	((512UL << 30) - MODULES_LEN)
extern struct page *vmemmap;
extern unsigned long vmemmap_size;

extern unsigned long MODULES_VADDR;
extern unsigned long MODULES_END;
#define MODULES_VADDR	MODULES_VADDR
#define MODULES_END	MODULES_END
#define MODULES_LEN	(1UL << 31)

static inline int is_module_addr(void *addr)
{
	BUILD_BUG_ON(MODULES_LEN > (1UL << 31));
	if (addr < (void *)MODULES_VADDR)
		return 0;
	if (addr > (void *)MODULES_END)
		return 0;
	return 1;
}

#ifdef CONFIG_KMSAN
#define KMSAN_VMALLOC_SIZE (VMALLOC_END - VMALLOC_START)
#define KMSAN_VMALLOC_SHADOW_START VMALLOC_END
#define KMSAN_VMALLOC_SHADOW_END (KMSAN_VMALLOC_SHADOW_START + KMSAN_VMALLOC_SIZE)
#define KMSAN_VMALLOC_ORIGIN_START KMSAN_VMALLOC_SHADOW_END
#define KMSAN_VMALLOC_ORIGIN_END (KMSAN_VMALLOC_ORIGIN_START + KMSAN_VMALLOC_SIZE)
#define KMSAN_MODULES_SHADOW_START KMSAN_VMALLOC_ORIGIN_END
#define KMSAN_MODULES_SHADOW_END (KMSAN_MODULES_SHADOW_START + MODULES_LEN)
#define KMSAN_MODULES_ORIGIN_START KMSAN_MODULES_SHADOW_END
#define KMSAN_MODULES_ORIGIN_END (KMSAN_MODULES_ORIGIN_START + MODULES_LEN)
#endif

#ifdef CONFIG_RANDOMIZE_BASE
#define KASLR_LEN	(1UL << 31)
#else
#define KASLR_LEN	0UL
#endif

void setup_protection_map(void);

/*
 * A 64 bit pagetable entry of S390 has following format:
 * |			 PFRA			      |0IPC|  OS  |
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 *
 * I Page-Invalid Bit:    Page is not available for address-translation
 * P Page-Protection Bit: Store access not possible for page
 * C Change-bit override: HW is not required to set change bit
 *
 * A 64 bit segmenttable entry of S390 has following format:
 * |        P-table origin                              |      TT
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 *
 * I Segment-Invalid Bit:    Segment is not available for address-translation
 * C Common-Segment Bit:     Segment is not private (PoP 3-30)
 * P Page-Protection Bit: Store access not possible for page
 * TT Type 00
 *
 * A 64 bit region table entry of S390 has following format:
 * |        S-table origin                             |   TF  TTTL
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 *
 * I Segment-Invalid Bit:    Segment is not available for address-translation
 * TT Type 01
 * TF
 * TL Table length
 *
 * The 64 bit regiontable origin of S390 has following format:
 * |      region table origon                          |       DTTL
 * 0000000000111111111122222222223333333333444444444455555555556666
 * 0123456789012345678901234567890123456789012345678901234567890123
 *
 * X Space-Switch event:
 * G Segment-Invalid Bit:  
 * P Private-Space Bit:    
 * S Storage-Alteration:
 * R Real space
 * TL Table-Length:
 *
 * A storage key has the following format:
 * | ACC |F|R|C|0|
 *  0   3 4 5 6 7
 * ACC: access key
 * F  : fetch protection bit
 * R  : referenced bit
 * C  : changed bit
 */

/* Hardware bits in the page table entry */
#define _PAGE_NOEXEC	0x100		/* HW no-execute bit  */
#define _PAGE_PROTECT	0x200		/* HW read-only bit  */
#define _PAGE_INVALID	0x400		/* HW invalid bit    */
#define _PAGE_LARGE	0x800		/* Bit to mark a large pte */

/* Software bits in the page table entry */
#define _PAGE_PRESENT	0x001		/* SW pte present bit */
#define _PAGE_YOUNG	0x004		/* SW pte young bit */
#define _PAGE_DIRTY	0x008		/* SW pte dirty bit */
#define _PAGE_READ	0x010		/* SW pte read bit */
#define _PAGE_WRITE	0x020		/* SW pte write bit */
#define _PAGE_SPECIAL	0x040		/* SW associated with special page */
#define _PAGE_UNUSED	0x080		/* SW bit for pgste usage state */

#ifdef CONFIG_MEM_SOFT_DIRTY
#define _PAGE_SOFT_DIRTY 0x002		/* SW pte soft dirty bit */
#else
#define _PAGE_SOFT_DIRTY 0x000
#endif

#define _PAGE_SW_BITS	0xffUL		/* All SW bits */

#define _PAGE_SWP_EXCLUSIVE _PAGE_LARGE	/* SW pte exclusive swap bit */

/* Set of bits not changed in pte_modify */
#define _PAGE_CHG_MASK		(PAGE_MASK | _PAGE_SPECIAL | _PAGE_DIRTY | \
				 _PAGE_YOUNG | _PAGE_SOFT_DIRTY)

/*
 * Mask of bits that must not be changed with RDP. Allow only _PAGE_PROTECT
 * HW bit and all SW bits.
 */
#define _PAGE_RDP_MASK		~(_PAGE_PROTECT | _PAGE_SW_BITS)

/*
 * handle_pte_fault uses pte_present and pte_none to find out the pte type
 * WITHOUT holding the page table lock. The _PAGE_PRESENT bit is used to
 * distinguish present from not-present ptes. It is changed only with the page
 * table lock held.
 *
 * The following table gives the different possible bit combinations for
 * the pte hardware and software bits in the last 12 bits of a pte
 * (. unassigned bit, x don't care, t swap type):
 *
 *				842100000000
 *				000084210000
 *				000000008421
 *				.IR.uswrdy.p
 * empty			.10.00000000
 * swap				.11..ttttt.0
 * prot-none, clean, old	.11.xx0000.1
 * prot-none, clean, young	.11.xx0001.1
 * prot-none, dirty, old	.11.xx0010.1
 * prot-none, dirty, young	.11.xx0011.1
 * read-only, clean, old	.11.xx0100.1
 * read-only, clean, young	.01.xx0101.1
 * read-only, dirty, old	.11.xx0110.1
 * read-only, dirty, young	.01.xx0111.1
 * read-write, clean, old	.11.xx1100.1
 * read-write, clean, young	.01.xx1101.1
 * read-write, dirty, old	.10.xx1110.1
 * read-write, dirty, young	.00.xx1111.1
 * HW-bits: R read-only, I invalid
 * SW-bits: p present, y young, d dirty, r read, w write, s special,
 *	    u unused, l large
 *
 * pte_none    is true for the bit pattern .10.00000000, pte == 0x400
 * pte_swap    is true for the bit pattern .11..ooooo.0, (pte & 0x201) == 0x200
 * pte_present is true for the bit pattern .xx.xxxxxx.1, (pte & 0x001) == 0x001
 */

/* Bits in the segment/region table address-space-control-element */
#define _ASCE_ORIGIN		~0xfffUL/* region/segment table origin	    */
#define _ASCE_PRIVATE_SPACE	0x100	/* private space control	    */
#define _ASCE_ALT_EVENT		0x80	/* storage alteration event control */
#define _ASCE_SPACE_SWITCH	0x40	/* space switch event		    */
#define _ASCE_REAL_SPACE	0x20	/* real space control		    */
#define _ASCE_TYPE_MASK		0x0c	/* asce table type mask		    */
#define _ASCE_TYPE_REGION1	0x0c	/* region first table type	    */
#define _ASCE_TYPE_REGION2	0x08	/* region second table type	    */
#define _ASCE_TYPE_REGION3	0x04	/* region third table type	    */
#define _ASCE_TYPE_SEGMENT	0x00	/* segment table type		    */
#define _ASCE_TABLE_LENGTH	0x03	/* region table length		    */

/* Bits in the region table entry */
#define _REGION_ENTRY_ORIGIN	~0xfffUL/* region/segment table origin	    */
#define _REGION_ENTRY_PROTECT	0x200	/* region protection bit	    */
#define _REGION_ENTRY_NOEXEC	0x100	/* region no-execute bit	    */
#define _REGION_ENTRY_OFFSET	0xc0	/* region table offset		    */
#define _REGION_ENTRY_INVALID	0x20	/* invalid region table entry	    */
#define _REGION_ENTRY_TYPE_MASK	0x0c	/* region table type mask	    */
#define _REGION_ENTRY_TYPE_R1	0x0c	/* region first table type	    */
#define _REGION_ENTRY_TYPE_R2	0x08	/* region second table type	    */
#define _REGION_ENTRY_TYPE_R3	0x04	/* region third table type	    */
#define _REGION_ENTRY_LENGTH	0x03	/* region third length		    */

#define _REGION1_ENTRY		(_REGION_ENTRY_TYPE_R1 | _REGION_ENTRY_LENGTH)
#define _REGION1_ENTRY_EMPTY	(_REGION_ENTRY_TYPE_R1 | _REGION_ENTRY_INVALID)
#define _REGION2_ENTRY		(_REGION_ENTRY_TYPE_R2 | _REGION_ENTRY_LENGTH)
#define _REGION2_ENTRY_EMPTY	(_REGION_ENTRY_TYPE_R2 | _REGION_ENTRY_INVALID)
#define _REGION3_ENTRY		(_REGION_ENTRY_TYPE_R3 | _REGION_ENTRY_LENGTH | \
				 _REGION3_ENTRY_PRESENT)
#define _REGION3_ENTRY_EMPTY	(_REGION_ENTRY_TYPE_R3 | _REGION_ENTRY_INVALID)

#define _REGION3_ENTRY_HARDWARE_BITS		0xfffffffffffff6ffUL
#define _REGION3_ENTRY_HARDWARE_BITS_LARGE	0xffffffff8001073cUL
#define _REGION3_ENTRY_ORIGIN_LARGE ~0x7fffffffUL /* large page address	     */
#define _REGION3_ENTRY_DIRTY	0x2000	/* SW region dirty bit */
#define _REGION3_ENTRY_YOUNG	0x1000	/* SW region young bit */
#define _REGION3_ENTRY_COMM	0x0010	/* Common-Region, marks swap entry */
#define _REGION3_ENTRY_LARGE	0x0400	/* RTTE-format control, large page  */
#define _REGION3_ENTRY_WRITE	0x8000	/* SW region write bit */
#define _REGION3_ENTRY_READ	0x4000	/* SW region read bit */

#ifdef CONFIG_MEM_SOFT_DIRTY
#define _REGION3_ENTRY_SOFT_DIRTY 0x0002 /* SW region soft dirty bit */
#else
#define _REGION3_ENTRY_SOFT_DIRTY 0x0000 /* SW region soft dirty bit */
#endif

#define _REGION_ENTRY_BITS	 0xfffffffffffff22fUL

/*
 * SW region present bit. For non-leaf region-third-table entries, bits 62-63
 * indicate the TABLE LENGTH and both must be set to 1. But such entries
 * would always be considered as present, so it is safe to use bit 63 as
 * PRESENT bit for PUD.
 */
#define _REGION3_ENTRY_PRESENT	0x0001

/* Bits in the segment table entry */
#define _SEGMENT_ENTRY_BITS			0xfffffffffffffe3fUL
#define _SEGMENT_ENTRY_HARDWARE_BITS		0xfffffffffffffe3cUL
#define _SEGMENT_ENTRY_HARDWARE_BITS_LARGE	0xfffffffffff1073cUL
#define _SEGMENT_ENTRY_ORIGIN_LARGE ~0xfffffUL /* large page address	    */
#define _SEGMENT_ENTRY_ORIGIN	~0x7ffUL/* page table origin		    */
#define _SEGMENT_ENTRY_PROTECT	0x200	/* segment protection bit	    */
#define _SEGMENT_ENTRY_NOEXEC	0x100	/* segment no-execute bit	    */
#define _SEGMENT_ENTRY_INVALID	0x20	/* invalid segment table entry	    */
#define _SEGMENT_ENTRY_TYPE_MASK 0x0c	/* segment table type mask	    */

#define _SEGMENT_ENTRY		(_SEGMENT_ENTRY_PRESENT)
#define _SEGMENT_ENTRY_EMPTY	(_SEGMENT_ENTRY_INVALID)

#define _SEGMENT_ENTRY_DIRTY	0x2000	/* SW segment dirty bit */
#define _SEGMENT_ENTRY_YOUNG	0x1000	/* SW segment young bit */

#define _SEGMENT_ENTRY_COMM	0x0010	/* Common-Segment, marks swap entry */
#define _SEGMENT_ENTRY_LARGE	0x0400	/* STE-format control, large page */
#define _SEGMENT_ENTRY_WRITE	0x8000	/* SW segment write bit */
#define _SEGMENT_ENTRY_READ	0x4000	/* SW segment read bit */

#ifdef CONFIG_MEM_SOFT_DIRTY
#define _SEGMENT_ENTRY_SOFT_DIRTY 0x0002 /* SW segment soft dirty bit */
#else
#define _SEGMENT_ENTRY_SOFT_DIRTY 0x0000 /* SW segment soft dirty bit */
#endif

#define _SEGMENT_ENTRY_PRESENT	0x0001	/* SW segment present bit */

/* Common bits in region and segment table entries, for swap entries */
#define _RST_ENTRY_COMM		0x0010	/* Common-Region/Segment, marks swap entry */
#define _RST_ENTRY_INVALID	0x0020	/* invalid region/segment table entry */

#define _CRST_ENTRIES	2048	/* number of region/segment table entries */
#define _PAGE_ENTRIES	256	/* number of page table entries	*/

#define _CRST_TABLE_SIZE (_CRST_ENTRIES * 8)
#define _PAGE_TABLE_SIZE (_PAGE_ENTRIES * 8)

#define _REGION1_SHIFT	53
#define _REGION2_SHIFT	42
#define _REGION3_SHIFT	31
#define _SEGMENT_SHIFT	20

#define _REGION1_INDEX	(0x7ffUL << _REGION1_SHIFT)
#define _REGION2_INDEX	(0x7ffUL << _REGION2_SHIFT)
#define _REGION3_INDEX	(0x7ffUL << _REGION3_SHIFT)
#define _SEGMENT_INDEX	(0x7ffUL << _SEGMENT_SHIFT)
#define _PAGE_INDEX	(0xffUL  << PAGE_SHIFT)

#define _REGION1_SIZE	(1UL << _REGION1_SHIFT)
#define _REGION2_SIZE	(1UL << _REGION2_SHIFT)
#define _REGION3_SIZE	(1UL << _REGION3_SHIFT)
#define _SEGMENT_SIZE	(1UL << _SEGMENT_SHIFT)

#define _REGION1_MASK	(~(_REGION1_SIZE - 1))
#define _REGION2_MASK	(~(_REGION2_SIZE - 1))
#define _REGION3_MASK	(~(_REGION3_SIZE - 1))
#define _SEGMENT_MASK	(~(_SEGMENT_SIZE - 1))

#define PMD_SHIFT	_SEGMENT_SHIFT
#define PUD_SHIFT	_REGION3_SHIFT
#define P4D_SHIFT	_REGION2_SHIFT
#define PGDIR_SHIFT	_REGION1_SHIFT

#define PMD_SIZE	_SEGMENT_SIZE
#define PUD_SIZE	_REGION3_SIZE
#define P4D_SIZE	_REGION2_SIZE
#define PGDIR_SIZE	_REGION1_SIZE

#define PMD_MASK	_SEGMENT_MASK
#define PUD_MASK	_REGION3_MASK
#define P4D_MASK	_REGION2_MASK
#define PGDIR_MASK	_REGION1_MASK

#define PTRS_PER_PTE	_PAGE_ENTRIES
#define PTRS_PER_PMD	_CRST_ENTRIES
#define PTRS_PER_PUD	_CRST_ENTRIES
#define PTRS_PER_P4D	_CRST_ENTRIES
#define PTRS_PER_PGD	_CRST_ENTRIES

/*
 * Segment table and region3 table entry encoding
 * (R = read-only, I = invalid, y = young bit):
 *				dy..R...I...wr
 * prot-none, clean, old	00..1...1...00
 * prot-none, clean, young	01..1...1...00
 * prot-none, dirty, old	10..1...1...00
 * prot-none, dirty, young	11..1...1...00
 * read-only, clean, old	00..1...1...01
 * read-only, clean, young	01..1...0...01
 * read-only, dirty, old	10..1...1...01
 * read-only, dirty, young	11..1...0...01
 * read-write, clean, old	00..1...1...11
 * read-write, clean, young	01..1...0...11
 * read-write, dirty, old	10..0...1...11
 * read-write, dirty, young	11..0...0...11
 * The segment table origin is used to distinguish empty (origin==0) from
 * read-write, old segment table entries (origin!=0)
 * HW-bits: R read-only, I invalid
 * SW-bits: y young, d dirty, r read, w write
 */

/* Page status table bits for virtualization */
#define PGSTE_ACC_BITS	0xf000000000000000UL
#define PGSTE_FP_BIT	0x0800000000000000UL
#define PGSTE_PCL_BIT	0x0080000000000000UL
#define PGSTE_HR_BIT	0x0040000000000000UL
#define PGSTE_HC_BIT	0x0020000000000000UL
#define PGSTE_GR_BIT	0x0004000000000000UL
#define PGSTE_GC_BIT	0x0002000000000000UL
#define PGSTE_ST2_MASK	0x0000ffff00000000UL
#define PGSTE_UC_BIT	0x0000000000008000UL	/* user dirty (migration) */
#define PGSTE_IN_BIT	0x0000000000004000UL	/* IPTE notify bit */
#define PGSTE_VSIE_BIT	0x0000000000002000UL	/* ref'd in a shadow table */

/* Guest Page State used for virtualization */
#define _PGSTE_GPS_ZERO			0x0000000080000000UL
#define _PGSTE_GPS_NODAT		0x0000000040000000UL
#define _PGSTE_GPS_USAGE_MASK		0x0000000003000000UL
#define _PGSTE_GPS_USAGE_STABLE		0x0000000000000000UL
#define _PGSTE_GPS_USAGE_UNUSED		0x0000000001000000UL
#define _PGSTE_GPS_USAGE_POT_VOLATILE	0x0000000002000000UL
#define _PGSTE_GPS_USAGE_VOLATILE	_PGSTE_GPS_USAGE_MASK

/*
 * A user page table pointer has the space-switch-event bit, the
 * private-space-control bit and the storage-alteration-event-control
 * bit set. A kernel page table pointer doesn't need them.
 */
#define _ASCE_USER_BITS		(_ASCE_SPACE_SWITCH | _ASCE_PRIVATE_SPACE | \
				 _ASCE_ALT_EVENT)

/*
 * Page protection definitions.
 */
#define __PAGE_NONE		(_PAGE_PRESENT | _PAGE_INVALID | _PAGE_PROTECT)
#define __PAGE_RO		(_PAGE_PRESENT | _PAGE_READ | \
				 _PAGE_NOEXEC  | _PAGE_INVALID | _PAGE_PROTECT)
#define __PAGE_RX		(_PAGE_PRESENT | _PAGE_READ | \
				 _PAGE_INVALID | _PAGE_PROTECT)
#define __PAGE_RW		(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | \
				 _PAGE_NOEXEC  | _PAGE_INVALID | _PAGE_PROTECT)
#define __PAGE_RWX		(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | \
				 _PAGE_INVALID | _PAGE_PROTECT)
#define __PAGE_SHARED		(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | \
				 _PAGE_YOUNG | _PAGE_DIRTY | _PAGE_NOEXEC)
#define __PAGE_KERNEL		(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | \
				 _PAGE_YOUNG | _PAGE_DIRTY | _PAGE_NOEXEC)
#define __PAGE_KERNEL_RO	(_PAGE_PRESENT | _PAGE_READ | _PAGE_YOUNG | \
				 _PAGE_PROTECT | _PAGE_NOEXEC)

extern unsigned long page_noexec_mask;

#define __pgprot_page_mask(x)	__pgprot((x) & page_noexec_mask)

#define PAGE_NONE		__pgprot_page_mask(__PAGE_NONE)
#define PAGE_RO			__pgprot_page_mask(__PAGE_RO)
#define PAGE_RX			__pgprot_page_mask(__PAGE_RX)
#define PAGE_RW			__pgprot_page_mask(__PAGE_RW)
#define PAGE_RWX		__pgprot_page_mask(__PAGE_RWX)
#define PAGE_SHARED		__pgprot_page_mask(__PAGE_SHARED)
#define PAGE_KERNEL		__pgprot_page_mask(__PAGE_KERNEL)
#define PAGE_KERNEL_RO		__pgprot_page_mask(__PAGE_KERNEL_RO)

/*
 * Segment entry (large page) protection definitions.
 */
#define __SEGMENT_NONE		(_SEGMENT_ENTRY_PRESENT | \
				 _SEGMENT_ENTRY_INVALID | \
				 _SEGMENT_ENTRY_PROTECT)
#define __SEGMENT_RO		(_SEGMENT_ENTRY_PRESENT | \
				 _SEGMENT_ENTRY_PROTECT | \
				 _SEGMENT_ENTRY_READ | \
				 _SEGMENT_ENTRY_NOEXEC)
#define __SEGMENT_RX		(_SEGMENT_ENTRY_PRESENT | \
				 _SEGMENT_ENTRY_PROTECT | \
				 _SEGMENT_ENTRY_READ)
#define __SEGMENT_RW		(_SEGMENT_ENTRY_PRESENT | \
				 _SEGMENT_ENTRY_READ | \
				 _SEGMENT_ENTRY_WRITE | \
				 _SEGMENT_ENTRY_NOEXEC)
#define __SEGMENT_RWX		(_SEGMENT_ENTRY_PRESENT | \
				 _SEGMENT_ENTRY_READ | \
				 _SEGMENT_ENTRY_WRITE)
#define __SEGMENT_KERNEL	(_SEGMENT_ENTRY |	\
				 _SEGMENT_ENTRY_LARGE |	\
				 _SEGMENT_ENTRY_READ |	\
				 _SEGMENT_ENTRY_WRITE | \
				 _SEGMENT_ENTRY_YOUNG | \
				 _SEGMENT_ENTRY_DIRTY | \
				 _SEGMENT_ENTRY_NOEXEC)
#define __SEGMENT_KERNEL_RO	(_SEGMENT_ENTRY |	\
				 _SEGMENT_ENTRY_LARGE |	\
				 _SEGMENT_ENTRY_READ |	\
				 _SEGMENT_ENTRY_YOUNG |	\
				 _SEGMENT_ENTRY_PROTECT | \
				 _SEGMENT_ENTRY_NOEXEC)

extern unsigned long segment_noexec_mask;

#define __pgprot_segment_mask(x) __pgprot((x) & segment_noexec_mask)

#define SEGMENT_NONE		__pgprot_segment_mask(__SEGMENT_NONE)
#define SEGMENT_RO		__pgprot_segment_mask(__SEGMENT_RO)
#define SEGMENT_RX		__pgprot_segment_mask(__SEGMENT_RX)
#define SEGMENT_RW		__pgprot_segment_mask(__SEGMENT_RW)
#define SEGMENT_RWX		__pgprot_segment_mask(__SEGMENT_RWX)
#define SEGMENT_KERNEL		__pgprot_segment_mask(__SEGMENT_KERNEL)
#define SEGMENT_KERNEL_RO	__pgprot_segment_mask(__SEGMENT_KERNEL_RO)

/*
 * Region3 entry (large page) protection definitions.
 */

#define __REGION3_KERNEL	(_REGION_ENTRY_TYPE_R3 | \
				 _REGION3_ENTRY_PRESENT | \
				 _REGION3_ENTRY_LARGE | \
				 _REGION3_ENTRY_READ | \
				 _REGION3_ENTRY_WRITE | \
				 _REGION3_ENTRY_YOUNG | \
				 _REGION3_ENTRY_DIRTY | \
				 _REGION_ENTRY_NOEXEC)
#define __REGION3_KERNEL_RO	(_REGION_ENTRY_TYPE_R3 | \
				 _REGION3_ENTRY_PRESENT | \
				 _REGION3_ENTRY_LARGE | \
				 _REGION3_ENTRY_READ | \
				 _REGION3_ENTRY_YOUNG | \
				 _REGION_ENTRY_PROTECT | \
				 _REGION_ENTRY_NOEXEC)

extern unsigned long region_noexec_mask;

#define __pgprot_region_mask(x)	__pgprot((x) & region_noexec_mask)

#define REGION3_KERNEL		__pgprot_region_mask(__REGION3_KERNEL)
#define REGION3_KERNEL_RO	__pgprot_region_mask(__REGION3_KERNEL_RO)

static inline bool mm_p4d_folded(struct mm_struct *mm)
{
	return mm->context.asce_limit <= _REGION1_SIZE;
}
#define mm_p4d_folded(mm) mm_p4d_folded(mm)

static inline bool mm_pud_folded(struct mm_struct *mm)
{
	return mm->context.asce_limit <= _REGION2_SIZE;
}
#define mm_pud_folded(mm) mm_pud_folded(mm)

static inline bool mm_pmd_folded(struct mm_struct *mm)
{
	return mm->context.asce_limit <= _REGION3_SIZE;
}
#define mm_pmd_folded(mm) mm_pmd_folded(mm)

static inline int mm_has_pgste(struct mm_struct *mm)
{
#ifdef CONFIG_PGSTE
	if (unlikely(mm->context.has_pgste))
		return 1;
#endif
	return 0;
}

static inline int mm_is_protected(struct mm_struct *mm)
{
#ifdef CONFIG_PGSTE
	if (unlikely(atomic_read(&mm->context.protected_count)))
		return 1;
#endif
	return 0;
}

static inline pgste_t clear_pgste_bit(pgste_t pgste, unsigned long mask)
{
	return __pgste(pgste_val(pgste) & ~mask);
}

static inline pgste_t set_pgste_bit(pgste_t pgste, unsigned long mask)
{
	return __pgste(pgste_val(pgste) | mask);
}

static inline pte_t clear_pte_bit(pte_t pte, pgprot_t prot)
{
	return __pte(pte_val(pte) & ~pgprot_val(prot));
}

static inline pte_t set_pte_bit(pte_t pte, pgprot_t prot)
{
	return __pte(pte_val(pte) | pgprot_val(prot));
}

static inline pmd_t clear_pmd_bit(pmd_t pmd, pgprot_t prot)
{
	return __pmd(pmd_val(pmd) & ~pgprot_val(prot));
}

static inline pmd_t set_pmd_bit(pmd_t pmd, pgprot_t prot)
{
	return __pmd(pmd_val(pmd) | pgprot_val(prot));
}

static inline pud_t clear_pud_bit(pud_t pud, pgprot_t prot)
{
	return __pud(pud_val(pud) & ~pgprot_val(prot));
}

static inline pud_t set_pud_bit(pud_t pud, pgprot_t prot)
{
	return __pud(pud_val(pud) | pgprot_val(prot));
}

/*
 * As soon as the guest uses storage keys or enables PV, we deduplicate all
 * mapped shared zeropages and prevent new shared zeropages from getting
 * mapped.
 */
#define mm_forbids_zeropage mm_forbids_zeropage
static inline int mm_forbids_zeropage(struct mm_struct *mm)
{
#ifdef CONFIG_PGSTE
	if (!mm->context.allow_cow_sharing)
		return 1;
#endif
	return 0;
}

static inline int mm_uses_skeys(struct mm_struct *mm)
{
#ifdef CONFIG_PGSTE
	if (mm->context.uses_skeys)
		return 1;
#endif
	return 0;
}

static inline void csp(unsigned int *ptr, unsigned int old, unsigned int new)
{
	union register_pair r1 = { .even = old, .odd = new, };
	unsigned long address = (unsigned long)ptr | 1;

	asm volatile(
		"	csp	%[r1],%[address]"
		: [r1] "+&d" (r1.pair), "+m" (*ptr)
		: [address] "d" (address)
		: "cc");
}

/**
 * cspg() - Compare and Swap and Purge (CSPG)
 * @ptr: Pointer to the value to be exchanged
 * @old: The expected old value
 * @new: The new value
 *
 * Return: True if compare and swap was successful, otherwise false.
 */
static inline bool cspg(unsigned long *ptr, unsigned long old, unsigned long new)
{
	union register_pair r1 = { .even = old, .odd = new, };
	unsigned long address = (unsigned long)ptr | 1;

	asm volatile(
		"	cspg	%[r1],%[address]"
		: [r1] "+&d" (r1.pair), "+m" (*ptr)
		: [address] "d" (address)
		: "cc");
	return old == r1.even;
}

#define CRDTE_DTT_PAGE		0x00UL
#define CRDTE_DTT_SEGMENT	0x10UL
#define CRDTE_DTT_REGION3	0x14UL
#define CRDTE_DTT_REGION2	0x18UL
#define CRDTE_DTT_REGION1	0x1cUL

/**
 * crdte() - Compare and Replace DAT Table Entry
 * @old:     The expected old value
 * @new:     The new value
 * @table:   Pointer to the value to be exchanged
 * @dtt:     Table type of the table to be exchanged
 * @address: The address mapped by the entry to be replaced
 * @asce:    The ASCE of this entry
 *
 * Return: True if compare and replace was successful, otherwise false.
 */
static inline bool crdte(unsigned long old, unsigned long new,
			 unsigned long *table, unsigned long dtt,
			 unsigned long address, unsigned long asce)
{
	union register_pair r1 = { .even = old, .odd = new, };
	union register_pair r2 = { .even = __pa(table) | dtt, .odd = address, };

	asm volatile(".insn rrf,0xb98f0000,%[r1],%[r2],%[asce],0"
		     : [r1] "+&d" (r1.pair)
		     : [r2] "d" (r2.pair), [asce] "a" (asce)
		     : "memory", "cc");
	return old == r1.even;
}

/*
 * pgd/p4d/pud/pmd/pte query functions
 */
static inline int pgd_folded(pgd_t pgd)
{
	return (pgd_val(pgd) & _REGION_ENTRY_TYPE_MASK) < _REGION_ENTRY_TYPE_R1;
}

static inline int pgd_present(pgd_t pgd)
{
	if (pgd_folded(pgd))
		return 1;
	return (pgd_val(pgd) & _REGION_ENTRY_ORIGIN) != 0UL;
}

static inline int pgd_none(pgd_t pgd)
{
	if (pgd_folded(pgd))
		return 0;
	return (pgd_val(pgd) & _REGION_ENTRY_INVALID) != 0UL;
}

static inline int pgd_bad(pgd_t pgd)
{
	if ((pgd_val(pgd) & _REGION_ENTRY_TYPE_MASK) < _REGION_ENTRY_TYPE_R1)
		return 0;
	return (pgd_val(pgd) & ~_REGION_ENTRY_BITS) != 0;
}

static inline unsigned long pgd_pfn(pgd_t pgd)
{
	unsigned long origin_mask;

	origin_mask = _REGION_ENTRY_ORIGIN;
	return (pgd_val(pgd) & origin_mask) >> PAGE_SHIFT;
}

static inline int p4d_folded(p4d_t p4d)
{
	return (p4d_val(p4d) & _REGION_ENTRY_TYPE_MASK) < _REGION_ENTRY_TYPE_R2;
}

static inline int p4d_present(p4d_t p4d)
{
	if (p4d_folded(p4d))
		return 1;
	return (p4d_val(p4d) & _REGION_ENTRY_ORIGIN) != 0UL;
}

static inline int p4d_none(p4d_t p4d)
{
	if (p4d_folded(p4d))
		return 0;
	return p4d_val(p4d) == _REGION2_ENTRY_EMPTY;
}

static inline unsigned long p4d_pfn(p4d_t p4d)
{
	unsigned long origin_mask;

	origin_mask = _REGION_ENTRY_ORIGIN;
	return (p4d_val(p4d) & origin_mask) >> PAGE_SHIFT;
}

static inline int pud_folded(pud_t pud)
{
	return (pud_val(pud) & _REGION_ENTRY_TYPE_MASK) < _REGION_ENTRY_TYPE_R3;
}

static inline int pud_present(pud_t pud)
{
	if (pud_folded(pud))
		return 1;
	return (pud_val(pud) & _REGION3_ENTRY_PRESENT) != 0;
}

static inline int pud_none(pud_t pud)
{
	if (pud_folded(pud))
		return 0;
	return pud_val(pud) == _REGION3_ENTRY_EMPTY;
}

#define pud_leaf pud_leaf
static inline bool pud_leaf(pud_t pud)
{
	if ((pud_val(pud) & _REGION_ENTRY_TYPE_MASK) != _REGION_ENTRY_TYPE_R3)
		return 0;
	return (pud_present(pud) && (pud_val(pud) & _REGION3_ENTRY_LARGE) != 0);
}

static inline int pmd_present(pmd_t pmd)
{
	return (pmd_val(pmd) & _SEGMENT_ENTRY_PRESENT) != 0;
}

#define pmd_leaf pmd_leaf
static inline bool pmd_leaf(pmd_t pmd)
{
	return (pmd_present(pmd) && (pmd_val(pmd) & _SEGMENT_ENTRY_LARGE) != 0);
}

static inline int pmd_bad(pmd_t pmd)
{
	if ((pmd_val(pmd) & _SEGMENT_ENTRY_TYPE_MASK) > 0 || pmd_leaf(pmd))
		return 1;
	return (pmd_val(pmd) & ~_SEGMENT_ENTRY_BITS) != 0;
}

static inline int pud_bad(pud_t pud)
{
	unsigned long type = pud_val(pud) & _REGION_ENTRY_TYPE_MASK;

	if (type > _REGION_ENTRY_TYPE_R3 || pud_leaf(pud))
		return 1;
	if (type < _REGION_ENTRY_TYPE_R3)
		return 0;
	return (pud_val(pud) & ~_REGION_ENTRY_BITS) != 0;
}

static inline int p4d_bad(p4d_t p4d)
{
	unsigned long type = p4d_val(p4d) & _REGION_ENTRY_TYPE_MASK;

	if (type > _REGION_ENTRY_TYPE_R2)
		return 1;
	if (type < _REGION_ENTRY_TYPE_R2)
		return 0;
	return (p4d_val(p4d) & ~_REGION_ENTRY_BITS) != 0;
}

static inline int pmd_none(pmd_t pmd)
{
	return pmd_val(pmd) == _SEGMENT_ENTRY_EMPTY;
}

#define pmd_write pmd_write
static inline int pmd_write(pmd_t pmd)
{
	return (pmd_val(pmd) & _SEGMENT_ENTRY_WRITE) != 0;
}

#define pud_write pud_write
static inline int pud_write(pud_t pud)
{
	return (pud_val(pud) & _REGION3_ENTRY_WRITE) != 0;
}

#define pmd_dirty pmd_dirty
static inline int pmd_dirty(pmd_t pmd)
{
	return (pmd_val(pmd) & _SEGMENT_ENTRY_DIRTY) != 0;
}

#define pmd_young pmd_young
static inline int pmd_young(pmd_t pmd)
{
	return (pmd_val(pmd) & _SEGMENT_ENTRY_YOUNG) != 0;
}

static inline int pte_present(pte_t pte)
{
	/* Bit pattern: (pte & 0x001) == 0x001 */
	return (pte_val(pte) & _PAGE_PRESENT) != 0;
}

static inline int pte_none(pte_t pte)
{
	/* Bit pattern: pte == 0x400 */
	return pte_val(pte) == _PAGE_INVALID;
}

static inline int pte_swap(pte_t pte)
{
	/* Bit pattern: (pte & 0x201) == 0x200 */
	return (pte_val(pte) & (_PAGE_PROTECT | _PAGE_PRESENT))
		== _PAGE_PROTECT;
}

static inline int pte_special(pte_t pte)
{
	return (pte_val(pte) & _PAGE_SPECIAL);
}

#define __HAVE_ARCH_PTE_SAME
static inline int pte_same(pte_t a, pte_t b)
{
	return pte_val(a) == pte_val(b);
}

#ifdef CONFIG_NUMA_BALANCING
static inline int pte_protnone(pte_t pte)
{
	return pte_present(pte) && !(pte_val(pte) & _PAGE_READ);
}

static inline int pmd_protnone(pmd_t pmd)
{
	/* pmd_leaf(pmd) implies pmd_present(pmd) */
	return pmd_leaf(pmd) && !(pmd_val(pmd) & _SEGMENT_ENTRY_READ);
}
#endif

static inline bool pte_swp_exclusive(pte_t pte)
{
	return pte_val(pte) & _PAGE_SWP_EXCLUSIVE;
}

static inline pte_t pte_swp_mkexclusive(pte_t pte)
{
	return set_pte_bit(pte, __pgprot(_PAGE_SWP_EXCLUSIVE));
}

static inline pte_t pte_swp_clear_exclusive(pte_t pte)
{
	return clear_pte_bit(pte, __pgprot(_PAGE_SWP_EXCLUSIVE));
}

static inline int pte_soft_dirty(pte_t pte)
{
	return pte_val(pte) & _PAGE_SOFT_DIRTY;
}
#define pte_swp_soft_dirty pte_soft_dirty

static inline pte_t pte_mksoft_dirty(pte_t pte)
{
	return set_pte_bit(pte, __pgprot(_PAGE_SOFT_DIRTY));
}
#define pte_swp_mksoft_dirty pte_mksoft_dirty

static inline pte_t pte_clear_soft_dirty(pte_t pte)
{
	return clear_pte_bit(pte, __pgprot(_PAGE_SOFT_DIRTY));
}
#define pte_swp_clear_soft_dirty pte_clear_soft_dirty

static inline int pmd_soft_dirty(pmd_t pmd)
{
	return pmd_val(pmd) & _SEGMENT_ENTRY_SOFT_DIRTY;
}

static inline pmd_t pmd_mksoft_dirty(pmd_t pmd)
{
	return set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_SOFT_DIRTY));
}

static inline pmd_t pmd_clear_soft_dirty(pmd_t pmd)
{
	return clear_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_SOFT_DIRTY));
}

/*
 * query functions pte_write/pte_dirty/pte_young only work if
 * pte_present() is true. Undefined behaviour if not..
 */
static inline int pte_write(pte_t pte)
{
	return (pte_val(pte) & _PAGE_WRITE) != 0;
}

static inline int pte_dirty(pte_t pte)
{
	return (pte_val(pte) & _PAGE_DIRTY) != 0;
}

static inline int pte_young(pte_t pte)
{
	return (pte_val(pte) & _PAGE_YOUNG) != 0;
}

#define __HAVE_ARCH_PTE_UNUSED
static inline int pte_unused(pte_t pte)
{
	return pte_val(pte) & _PAGE_UNUSED;
}

/*
 * Extract the pgprot value from the given pte while at the same time making it
 * usable for kernel address space mappings where fault driven dirty and
 * young/old accounting is not supported, i.e _PAGE_PROTECT and _PAGE_INVALID
 * must not be set.
 */
#define pte_pgprot pte_pgprot
static inline pgprot_t pte_pgprot(pte_t pte)
{
	unsigned long pte_flags = pte_val(pte) & _PAGE_CHG_MASK;

	if (pte_write(pte))
		pte_flags |= pgprot_val(PAGE_KERNEL);
	else
		pte_flags |= pgprot_val(PAGE_KERNEL_RO);
	pte_flags |= pte_val(pte) & mio_wb_bit_mask;

	return __pgprot(pte_flags);
}

/*
 * pgd/pmd/pte modification functions
 */

static inline void set_pgd(pgd_t *pgdp, pgd_t pgd)
{
	WRITE_ONCE(*pgdp, pgd);
}

static inline void set_p4d(p4d_t *p4dp, p4d_t p4d)
{
	WRITE_ONCE(*p4dp, p4d);
}

static inline void set_pud(pud_t *pudp, pud_t pud)
{
	WRITE_ONCE(*pudp, pud);
}

static inline void set_pmd(pmd_t *pmdp, pmd_t pmd)
{
	WRITE_ONCE(*pmdp, pmd);
}

static inline void set_pte(pte_t *ptep, pte_t pte)
{
	WRITE_ONCE(*ptep, pte);
}

static inline void pgd_clear(pgd_t *pgd)
{
	if ((pgd_val(*pgd) & _REGION_ENTRY_TYPE_MASK) == _REGION_ENTRY_TYPE_R1)
		set_pgd(pgd, __pgd(_REGION1_ENTRY_EMPTY));
}

static inline void p4d_clear(p4d_t *p4d)
{
	if ((p4d_val(*p4d) & _REGION_ENTRY_TYPE_MASK) == _REGION_ENTRY_TYPE_R2)
		set_p4d(p4d, __p4d(_REGION2_ENTRY_EMPTY));
}

static inline void pud_clear(pud_t *pud)
{
	if ((pud_val(*pud) & _REGION_ENTRY_TYPE_MASK) == _REGION_ENTRY_TYPE_R3)
		set_pud(pud, __pud(_REGION3_ENTRY_EMPTY));
}

static inline void pmd_clear(pmd_t *pmdp)
{
	set_pmd(pmdp, __pmd(_SEGMENT_ENTRY_EMPTY));
}

static inline void pte_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	set_pte(ptep, __pte(_PAGE_INVALID));
}

/*
 * The following pte modification functions only work if
 * pte_present() is true. Undefined behaviour if not..
 */
static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte = clear_pte_bit(pte, __pgprot(~_PAGE_CHG_MASK));
	pte = set_pte_bit(pte, newprot);
	/*
	 * newprot for PAGE_NONE, PAGE_RO, PAGE_RX, PAGE_RW and PAGE_RWX
	 * has the invalid bit set, clear it again for readable, young pages
	 */
	if ((pte_val(pte) & _PAGE_YOUNG) && (pte_val(pte) & _PAGE_READ))
		pte = clear_pte_bit(pte, __pgprot(_PAGE_INVALID));
	/*
	 * newprot for PAGE_RO, PAGE_RX, PAGE_RW and PAGE_RWX has the page
	 * protection bit set, clear it again for writable, dirty pages
	 */
	if ((pte_val(pte) & _PAGE_DIRTY) && (pte_val(pte) & _PAGE_WRITE))
		pte = clear_pte_bit(pte, __pgprot(_PAGE_PROTECT));
	return pte;
}

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte = clear_pte_bit(pte, __pgprot(_PAGE_WRITE));
	return set_pte_bit(pte, __pgprot(_PAGE_PROTECT));
}

static inline pte_t pte_mkwrite_novma(pte_t pte)
{
	pte = set_pte_bit(pte, __pgprot(_PAGE_WRITE));
	if (pte_val(pte) & _PAGE_DIRTY)
		pte = clear_pte_bit(pte, __pgprot(_PAGE_PROTECT));
	return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte = clear_pte_bit(pte, __pgprot(_PAGE_DIRTY));
	return set_pte_bit(pte, __pgprot(_PAGE_PROTECT));
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte = set_pte_bit(pte, __pgprot(_PAGE_DIRTY | _PAGE_SOFT_DIRTY));
	if (pte_val(pte) & _PAGE_WRITE)
		pte = clear_pte_bit(pte, __pgprot(_PAGE_PROTECT));
	return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	pte = clear_pte_bit(pte, __pgprot(_PAGE_YOUNG));
	return set_pte_bit(pte, __pgprot(_PAGE_INVALID));
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	pte = set_pte_bit(pte, __pgprot(_PAGE_YOUNG));
	if (pte_val(pte) & _PAGE_READ)
		pte = clear_pte_bit(pte, __pgprot(_PAGE_INVALID));
	return pte;
}

static inline pte_t pte_mkspecial(pte_t pte)
{
	return set_pte_bit(pte, __pgprot(_PAGE_SPECIAL));
}

#ifdef CONFIG_HUGETLB_PAGE
static inline pte_t pte_mkhuge(pte_t pte)
{
	return set_pte_bit(pte, __pgprot(_PAGE_LARGE));
}
#endif

#define IPTE_GLOBAL	0
#define	IPTE_LOCAL	1

#define IPTE_NODAT	0x400
#define IPTE_GUEST_ASCE	0x800

static __always_inline void __ptep_rdp(unsigned long addr, pte_t *ptep,
				       unsigned long opt, unsigned long asce,
				       int local)
{
	unsigned long pto;

	pto = __pa(ptep) & ~(PTRS_PER_PTE * sizeof(pte_t) - 1);
	asm volatile(".insn rrf,0xb98b0000,%[r1],%[r2],%[asce],%[m4]"
		     : "+m" (*ptep)
		     : [r1] "a" (pto), [r2] "a" ((addr & PAGE_MASK) | opt),
		       [asce] "a" (asce), [m4] "i" (local));
}

static __always_inline void __ptep_ipte(unsigned long address, pte_t *ptep,
					unsigned long opt, unsigned long asce,
					int local)
{
	unsigned long pto = __pa(ptep);

	if (__builtin_constant_p(opt) && opt == 0) {
		/* Invalidation + TLB flush for the pte */
		asm volatile(
			"	ipte	%[r1],%[r2],0,%[m4]"
			: "+m" (*ptep) : [r1] "a" (pto), [r2] "a" (address),
			  [m4] "i" (local));
		return;
	}

	/* Invalidate ptes with options + TLB flush of the ptes */
	opt = opt | (asce & _ASCE_ORIGIN);
	asm volatile(
		"	ipte	%[r1],%[r2],%[r3],%[m4]"
		: [r2] "+a" (address), [r3] "+a" (opt)
		: [r1] "a" (pto), [m4] "i" (local) : "memory");
}

static __always_inline void __ptep_ipte_range(unsigned long address, int nr,
					      pte_t *ptep, int local)
{
	unsigned long pto = __pa(ptep);

	/* Invalidate a range of ptes + TLB flush of the ptes */
	do {
		asm volatile(
			"	ipte %[r1],%[r2],%[r3],%[m4]"
			: [r2] "+a" (address), [r3] "+a" (nr)
			: [r1] "a" (pto), [m4] "i" (local) : "memory");
	} while (nr != 255);
}

/*
 * This is hard to understand. ptep_get_and_clear and ptep_clear_flush
 * both clear the TLB for the unmapped pte. The reason is that
 * ptep_get_and_clear is used in common code (e.g. change_pte_range)
 * to modify an active pte. The sequence is
 *   1) ptep_get_and_clear
 *   2) set_pte_at
 *   3) flush_tlb_range
 * On s390 the tlb needs to get flushed with the modification of the pte
 * if the pte is active. The only way how this can be implemented is to
 * have ptep_get_and_clear do the tlb flush. In exchange flush_tlb_range
 * is a nop.
 */
pte_t ptep_xchg_direct(struct mm_struct *, unsigned long, pte_t *, pte_t);
pte_t ptep_xchg_lazy(struct mm_struct *, unsigned long, pte_t *, pte_t);

#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
static inline int ptep_test_and_clear_young(struct vm_area_struct *vma,
					    unsigned long addr, pte_t *ptep)
{
	pte_t pte = *ptep;

	pte = ptep_xchg_direct(vma->vm_mm, addr, ptep, pte_mkold(pte));
	return pte_young(pte);
}

#define __HAVE_ARCH_PTEP_CLEAR_YOUNG_FLUSH
static inline int ptep_clear_flush_young(struct vm_area_struct *vma,
					 unsigned long address, pte_t *ptep)
{
	return ptep_test_and_clear_young(vma, address, ptep);
}

#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
static inline pte_t ptep_get_and_clear(struct mm_struct *mm,
				       unsigned long addr, pte_t *ptep)
{
	pte_t res;

	res = ptep_xchg_lazy(mm, addr, ptep, __pte(_PAGE_INVALID));
	/* At this point the reference through the mapping is still present */
	if (mm_is_protected(mm) && pte_present(res))
		uv_convert_from_secure_pte(res);
	return res;
}

#define __HAVE_ARCH_PTEP_MODIFY_PROT_TRANSACTION
pte_t ptep_modify_prot_start(struct vm_area_struct *, unsigned long, pte_t *);
void ptep_modify_prot_commit(struct vm_area_struct *, unsigned long,
			     pte_t *, pte_t, pte_t);

#define __HAVE_ARCH_PTEP_CLEAR_FLUSH
static inline pte_t ptep_clear_flush(struct vm_area_struct *vma,
				     unsigned long addr, pte_t *ptep)
{
	pte_t res;

	res = ptep_xchg_direct(vma->vm_mm, addr, ptep, __pte(_PAGE_INVALID));
	/* At this point the reference through the mapping is still present */
	if (mm_is_protected(vma->vm_mm) && pte_present(res))
		uv_convert_from_secure_pte(res);
	return res;
}

/*
 * The batched pte unmap code uses ptep_get_and_clear_full to clear the
 * ptes. Here an optimization is possible. tlb_gather_mmu flushes all
 * tlbs of an mm if it can guarantee that the ptes of the mm_struct
 * cannot be accessed while the batched unmap is running. In this case
 * full==1 and a simple pte_clear is enough. See tlb.h.
 */
#define __HAVE_ARCH_PTEP_GET_AND_CLEAR_FULL
static inline pte_t ptep_get_and_clear_full(struct mm_struct *mm,
					    unsigned long addr,
					    pte_t *ptep, int full)
{
	pte_t res;

	if (full) {
		res = *ptep;
		set_pte(ptep, __pte(_PAGE_INVALID));
	} else {
		res = ptep_xchg_lazy(mm, addr, ptep, __pte(_PAGE_INVALID));
	}
	/* Nothing to do */
	if (!mm_is_protected(mm) || !pte_present(res))
		return res;
	/*
	 * At this point the reference through the mapping is still present.
	 * The notifier should have destroyed all protected vCPUs at this
	 * point, so the destroy should be successful.
	 */
	if (full && !uv_destroy_pte(res))
		return res;
	/*
	 * If something went wrong and the page could not be destroyed, or
	 * if this is not a mm teardown, the slower export is used as
	 * fallback instead.
	 */
	uv_convert_from_secure_pte(res);
	return res;
}

#define __HAVE_ARCH_PTEP_SET_WRPROTECT
static inline void ptep_set_wrprotect(struct mm_struct *mm,
				      unsigned long addr, pte_t *ptep)
{
	pte_t pte = *ptep;

	if (pte_write(pte))
		ptep_xchg_lazy(mm, addr, ptep, pte_wrprotect(pte));
}

/*
 * Check if PTEs only differ in _PAGE_PROTECT HW bit, but also allow SW PTE
 * bits in the comparison. Those might change e.g. because of dirty and young
 * tracking.
 */
static inline int pte_allow_rdp(pte_t old, pte_t new)
{
	/*
	 * Only allow changes from RO to RW
	 */
	if (!(pte_val(old) & _PAGE_PROTECT) || pte_val(new) & _PAGE_PROTECT)
		return 0;

	return (pte_val(old) & _PAGE_RDP_MASK) == (pte_val(new) & _PAGE_RDP_MASK);
}

static inline void flush_tlb_fix_spurious_fault(struct vm_area_struct *vma,
						unsigned long address,
						pte_t *ptep)
{
	/*
	 * RDP might not have propagated the PTE protection reset to all CPUs,
	 * so there could be spurious TLB protection faults.
	 * NOTE: This will also be called when a racing pagetable update on
	 * another thread already installed the correct PTE. Both cases cannot
	 * really be distinguished.
	 * Therefore, only do the local TLB flush when RDP can be used, and the
	 * PTE does not have _PAGE_PROTECT set, to avoid unnecessary overhead.
	 * A local RDP can be used to do the flush.
	 */
	if (cpu_has_rdp() && !(pte_val(*ptep) & _PAGE_PROTECT))
		__ptep_rdp(address, ptep, 0, 0, 1);
}
#define flush_tlb_fix_spurious_fault flush_tlb_fix_spurious_fault

void ptep_reset_dat_prot(struct mm_struct *mm, unsigned long addr, pte_t *ptep,
			 pte_t new);

#define __HAVE_ARCH_PTEP_SET_ACCESS_FLAGS
static inline int ptep_set_access_flags(struct vm_area_struct *vma,
					unsigned long addr, pte_t *ptep,
					pte_t entry, int dirty)
{
	if (pte_same(*ptep, entry))
		return 0;
	if (cpu_has_rdp() && !mm_has_pgste(vma->vm_mm) && pte_allow_rdp(*ptep, entry))
		ptep_reset_dat_prot(vma->vm_mm, addr, ptep, entry);
	else
		ptep_xchg_direct(vma->vm_mm, addr, ptep, entry);
	return 1;
}

/*
 * Additional functions to handle KVM guest page tables
 */
void ptep_set_pte_at(struct mm_struct *mm, unsigned long addr,
		     pte_t *ptep, pte_t entry);
void ptep_set_notify(struct mm_struct *mm, unsigned long addr, pte_t *ptep);
void ptep_notify(struct mm_struct *mm, unsigned long addr,
		 pte_t *ptep, unsigned long bits);
int ptep_force_prot(struct mm_struct *mm, unsigned long gaddr,
		    pte_t *ptep, int prot, unsigned long bit);
void ptep_zap_unused(struct mm_struct *mm, unsigned long addr,
		     pte_t *ptep , int reset);
void ptep_zap_key(struct mm_struct *mm, unsigned long addr, pte_t *ptep);
int ptep_shadow_pte(struct mm_struct *mm, unsigned long saddr,
		    pte_t *sptep, pte_t *tptep, pte_t pte);
void ptep_unshadow_pte(struct mm_struct *mm, unsigned long saddr, pte_t *ptep);

bool ptep_test_and_clear_uc(struct mm_struct *mm, unsigned long address,
			    pte_t *ptep);
int set_guest_storage_key(struct mm_struct *mm, unsigned long addr,
			  unsigned char key, bool nq);
int cond_set_guest_storage_key(struct mm_struct *mm, unsigned long addr,
			       unsigned char key, unsigned char *oldkey,
			       bool nq, bool mr, bool mc);
int reset_guest_reference_bit(struct mm_struct *mm, unsigned long addr);
int get_guest_storage_key(struct mm_struct *mm, unsigned long addr,
			  unsigned char *key);

int set_pgste_bits(struct mm_struct *mm, unsigned long addr,
				unsigned long bits, unsigned long value);
int get_pgste(struct mm_struct *mm, unsigned long hva, unsigned long *pgstep);
int pgste_perform_essa(struct mm_struct *mm, unsigned long hva, int orc,
			unsigned long *oldpte, unsigned long *oldpgste);
void gmap_pmdp_csp(struct mm_struct *mm, unsigned long vmaddr);
void gmap_pmdp_invalidate(struct mm_struct *mm, unsigned long vmaddr);
void gmap_pmdp_idte_local(struct mm_struct *mm, unsigned long vmaddr);
void gmap_pmdp_idte_global(struct mm_struct *mm, unsigned long vmaddr);

#define pgprot_writecombine	pgprot_writecombine
pgprot_t pgprot_writecombine(pgprot_t prot);

#define PFN_PTE_SHIFT		PAGE_SHIFT

/*
 * Set multiple PTEs to consecutive pages with a single call.  All PTEs
 * are within the same folio, PMD and VMA.
 */
static inline void set_ptes(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep, pte_t entry, unsigned int nr)
{
	if (pte_present(entry))
		entry = clear_pte_bit(entry, __pgprot(_PAGE_UNUSED));
	if (mm_has_pgste(mm)) {
		for (;;) {
			ptep_set_pte_at(mm, addr, ptep, entry);
			if (--nr == 0)
				break;
			ptep++;
			entry = __pte(pte_val(entry) + PAGE_SIZE);
			addr += PAGE_SIZE;
		}
	} else {
		for (;;) {
			set_pte(ptep, entry);
			if (--nr == 0)
				break;
			ptep++;
			entry = __pte(pte_val(entry) + PAGE_SIZE);
		}
	}
}
#define set_ptes set_ptes

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */
static inline pte_t mk_pte_phys(unsigned long physpage, pgprot_t pgprot)
{
	pte_t __pte;

	__pte = __pte(physpage | pgprot_val(pgprot));
	return pte_mkyoung(__pte);
}

#define pgd_index(address) (((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))
#define p4d_index(address) (((address) >> P4D_SHIFT) & (PTRS_PER_P4D-1))
#define pud_index(address) (((address) >> PUD_SHIFT) & (PTRS_PER_PUD-1))
#define pmd_index(address) (((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1))

#define p4d_deref(pud) ((unsigned long)__va(p4d_val(pud) & _REGION_ENTRY_ORIGIN))
#define pgd_deref(pgd) ((unsigned long)__va(pgd_val(pgd) & _REGION_ENTRY_ORIGIN))

static inline unsigned long pmd_deref(pmd_t pmd)
{
	unsigned long origin_mask;

	origin_mask = _SEGMENT_ENTRY_ORIGIN;
	if (pmd_leaf(pmd))
		origin_mask = _SEGMENT_ENTRY_ORIGIN_LARGE;
	return (unsigned long)__va(pmd_val(pmd) & origin_mask);
}

static inline unsigned long pmd_pfn(pmd_t pmd)
{
	return __pa(pmd_deref(pmd)) >> PAGE_SHIFT;
}

static inline unsigned long pud_deref(pud_t pud)
{
	unsigned long origin_mask;

	origin_mask = _REGION_ENTRY_ORIGIN;
	if (pud_leaf(pud))
		origin_mask = _REGION3_ENTRY_ORIGIN_LARGE;
	return (unsigned long)__va(pud_val(pud) & origin_mask);
}

#define pud_pfn pud_pfn
static inline unsigned long pud_pfn(pud_t pud)
{
	return __pa(pud_deref(pud)) >> PAGE_SHIFT;
}

/*
 * The pgd_offset function *always* adds the index for the top-level
 * region/segment table. This is done to get a sequence like the
 * following to work:
 *	pgdp = pgd_offset(current->mm, addr);
 *	pgd = READ_ONCE(*pgdp);
 *	p4dp = p4d_offset(&pgd, addr);
 *	...
 * The subsequent p4d_offset, pud_offset and pmd_offset functions
 * only add an index if they dereferenced the pointer.
 */
static inline pgd_t *pgd_offset_raw(pgd_t *pgd, unsigned long address)
{
	unsigned long rste;
	unsigned int shift;

	/* Get the first entry of the top level table */
	rste = pgd_val(*pgd);
	/* Pick up the shift from the table type of the first entry */
	shift = ((rste & _REGION_ENTRY_TYPE_MASK) >> 2) * 11 + 20;
	return pgd + ((address >> shift) & (PTRS_PER_PGD - 1));
}

#define pgd_offset(mm, address) pgd_offset_raw(READ_ONCE((mm)->pgd), address)

static inline p4d_t *p4d_offset_lockless(pgd_t *pgdp, pgd_t pgd, unsigned long address)
{
	if ((pgd_val(pgd) & _REGION_ENTRY_TYPE_MASK) >= _REGION_ENTRY_TYPE_R1)
		return (p4d_t *) pgd_deref(pgd) + p4d_index(address);
	return (p4d_t *) pgdp;
}
#define p4d_offset_lockless p4d_offset_lockless

static inline p4d_t *p4d_offset(pgd_t *pgdp, unsigned long address)
{
	return p4d_offset_lockless(pgdp, *pgdp, address);
}

static inline pud_t *pud_offset_lockless(p4d_t *p4dp, p4d_t p4d, unsigned long address)
{
	if ((p4d_val(p4d) & _REGION_ENTRY_TYPE_MASK) >= _REGION_ENTRY_TYPE_R2)
		return (pud_t *) p4d_deref(p4d) + pud_index(address);
	return (pud_t *) p4dp;
}
#define pud_offset_lockless pud_offset_lockless

static inline pud_t *pud_offset(p4d_t *p4dp, unsigned long address)
{
	return pud_offset_lockless(p4dp, *p4dp, address);
}
#define pud_offset pud_offset

static inline pmd_t *pmd_offset_lockless(pud_t *pudp, pud_t pud, unsigned long address)
{
	if ((pud_val(pud) & _REGION_ENTRY_TYPE_MASK) >= _REGION_ENTRY_TYPE_R3)
		return (pmd_t *) pud_deref(pud) + pmd_index(address);
	return (pmd_t *) pudp;
}
#define pmd_offset_lockless pmd_offset_lockless

static inline pmd_t *pmd_offset(pud_t *pudp, unsigned long address)
{
	return pmd_offset_lockless(pudp, *pudp, address);
}
#define pmd_offset pmd_offset

static inline unsigned long pmd_page_vaddr(pmd_t pmd)
{
	return (unsigned long) pmd_deref(pmd);
}

static inline bool gup_fast_permitted(unsigned long start, unsigned long end)
{
	return end <= current->mm->context.asce_limit;
}
#define gup_fast_permitted gup_fast_permitted

#define pfn_pte(pfn, pgprot)	mk_pte_phys(((pfn) << PAGE_SHIFT), (pgprot))
#define pte_pfn(x) (pte_val(x) >> PAGE_SHIFT)
#define pte_page(x) pfn_to_page(pte_pfn(x))

#define pmd_page(pmd) pfn_to_page(pmd_pfn(pmd))
#define pud_page(pud) pfn_to_page(pud_pfn(pud))
#define p4d_page(p4d) pfn_to_page(p4d_pfn(p4d))
#define pgd_page(pgd) pfn_to_page(pgd_pfn(pgd))

static inline pmd_t pmd_wrprotect(pmd_t pmd)
{
	pmd = clear_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_WRITE));
	return set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_PROTECT));
}

static inline pmd_t pmd_mkwrite_novma(pmd_t pmd)
{
	pmd = set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_WRITE));
	if (pmd_val(pmd) & _SEGMENT_ENTRY_DIRTY)
		pmd = clear_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_PROTECT));
	return pmd;
}

static inline pmd_t pmd_mkclean(pmd_t pmd)
{
	pmd = clear_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_DIRTY));
	return set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_PROTECT));
}

static inline pmd_t pmd_mkdirty(pmd_t pmd)
{
	pmd = set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_DIRTY | _SEGMENT_ENTRY_SOFT_DIRTY));
	if (pmd_val(pmd) & _SEGMENT_ENTRY_WRITE)
		pmd = clear_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_PROTECT));
	return pmd;
}

static inline pud_t pud_wrprotect(pud_t pud)
{
	pud = clear_pud_bit(pud, __pgprot(_REGION3_ENTRY_WRITE));
	return set_pud_bit(pud, __pgprot(_REGION_ENTRY_PROTECT));
}

static inline pud_t pud_mkwrite(pud_t pud)
{
	pud = set_pud_bit(pud, __pgprot(_REGION3_ENTRY_WRITE));
	if (pud_val(pud) & _REGION3_ENTRY_DIRTY)
		pud = clear_pud_bit(pud, __pgprot(_REGION_ENTRY_PROTECT));
	return pud;
}

static inline pud_t pud_mkclean(pud_t pud)
{
	pud = clear_pud_bit(pud, __pgprot(_REGION3_ENTRY_DIRTY));
	return set_pud_bit(pud, __pgprot(_REGION_ENTRY_PROTECT));
}

static inline pud_t pud_mkdirty(pud_t pud)
{
	pud = set_pud_bit(pud, __pgprot(_REGION3_ENTRY_DIRTY | _REGION3_ENTRY_SOFT_DIRTY));
	if (pud_val(pud) & _REGION3_ENTRY_WRITE)
		pud = clear_pud_bit(pud, __pgprot(_REGION_ENTRY_PROTECT));
	return pud;
}

#if defined(CONFIG_TRANSPARENT_HUGEPAGE) || defined(CONFIG_HUGETLB_PAGE)
static inline unsigned long massage_pgprot_pmd(pgprot_t pgprot)
{
	/*
	 * pgprot is PAGE_NONE, PAGE_RO, PAGE_RX, PAGE_RW or PAGE_RWX
	 * (see __Pxxx / __Sxxx). Convert to segment table entry format.
	 */
	if (pgprot_val(pgprot) == pgprot_val(PAGE_NONE))
		return pgprot_val(SEGMENT_NONE);
	if (pgprot_val(pgprot) == pgprot_val(PAGE_RO))
		return pgprot_val(SEGMENT_RO);
	if (pgprot_val(pgprot) == pgprot_val(PAGE_RX))
		return pgprot_val(SEGMENT_RX);
	if (pgprot_val(pgprot) == pgprot_val(PAGE_RW))
		return pgprot_val(SEGMENT_RW);
	return pgprot_val(SEGMENT_RWX);
}

static inline pmd_t pmd_mkyoung(pmd_t pmd)
{
	pmd = set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_YOUNG));
	if (pmd_val(pmd) & _SEGMENT_ENTRY_READ)
		pmd = clear_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_INVALID));
	return pmd;
}

static inline pmd_t pmd_mkold(pmd_t pmd)
{
	pmd = clear_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_YOUNG));
	return set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_INVALID));
}

static inline pmd_t pmd_modify(pmd_t pmd, pgprot_t newprot)
{
	unsigned long mask;

	mask  = _SEGMENT_ENTRY_ORIGIN_LARGE;
	mask |= _SEGMENT_ENTRY_DIRTY;
	mask |= _SEGMENT_ENTRY_YOUNG;
	mask |=	_SEGMENT_ENTRY_LARGE;
	mask |= _SEGMENT_ENTRY_SOFT_DIRTY;
	pmd = __pmd(pmd_val(pmd) & mask);
	pmd = set_pmd_bit(pmd, __pgprot(massage_pgprot_pmd(newprot)));
	if (!(pmd_val(pmd) & _SEGMENT_ENTRY_DIRTY))
		pmd = set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_PROTECT));
	if (!(pmd_val(pmd) & _SEGMENT_ENTRY_YOUNG))
		pmd = set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_INVALID));
	return pmd;
}

static inline pmd_t mk_pmd_phys(unsigned long physpage, pgprot_t pgprot)
{
	return __pmd(physpage + massage_pgprot_pmd(pgprot));
}

#endif /* CONFIG_TRANSPARENT_HUGEPAGE || CONFIG_HUGETLB_PAGE */

static inline void __pmdp_csp(pmd_t *pmdp)
{
	csp((unsigned int *)pmdp + 1, pmd_val(*pmdp),
	    pmd_val(*pmdp) | _SEGMENT_ENTRY_INVALID);
}

#define IDTE_GLOBAL	0
#define IDTE_LOCAL	1

#define IDTE_PTOA	0x0800
#define IDTE_NODAT	0x1000
#define IDTE_GUEST_ASCE	0x2000

static __always_inline void __pmdp_idte(unsigned long addr, pmd_t *pmdp,
					unsigned long opt, unsigned long asce,
					int local)
{
	unsigned long sto;

	sto = __pa(pmdp) - pmd_index(addr) * sizeof(pmd_t);
	if (__builtin_constant_p(opt) && opt == 0) {
		/* flush without guest asce */
		asm volatile(
			"	idte	%[r1],0,%[r2],%[m4]"
			: "+m" (*pmdp)
			: [r1] "a" (sto), [r2] "a" ((addr & HPAGE_MASK)),
			  [m4] "i" (local)
			: "cc" );
	} else {
		/* flush with guest asce */
		asm volatile(
			"	idte	%[r1],%[r3],%[r2],%[m4]"
			: "+m" (*pmdp)
			: [r1] "a" (sto), [r2] "a" ((addr & HPAGE_MASK) | opt),
			  [r3] "a" (asce), [m4] "i" (local)
			: "cc" );
	}
}

static __always_inline void __pudp_idte(unsigned long addr, pud_t *pudp,
					unsigned long opt, unsigned long asce,
					int local)
{
	unsigned long r3o;

	r3o = __pa(pudp) - pud_index(addr) * sizeof(pud_t);
	r3o |= _ASCE_TYPE_REGION3;
	if (__builtin_constant_p(opt) && opt == 0) {
		/* flush without guest asce */
		asm volatile(
			"	idte	%[r1],0,%[r2],%[m4]"
			: "+m" (*pudp)
			: [r1] "a" (r3o), [r2] "a" ((addr & PUD_MASK)),
			  [m4] "i" (local)
			: "cc");
	} else {
		/* flush with guest asce */
		asm volatile(
			"	idte	%[r1],%[r3],%[r2],%[m4]"
			: "+m" (*pudp)
			: [r1] "a" (r3o), [r2] "a" ((addr & PUD_MASK) | opt),
			  [r3] "a" (asce), [m4] "i" (local)
			: "cc" );
	}
}

pmd_t pmdp_xchg_direct(struct mm_struct *, unsigned long, pmd_t *, pmd_t);
pmd_t pmdp_xchg_lazy(struct mm_struct *, unsigned long, pmd_t *, pmd_t);
pud_t pudp_xchg_direct(struct mm_struct *, unsigned long, pud_t *, pud_t);

#ifdef CONFIG_TRANSPARENT_HUGEPAGE

#define __HAVE_ARCH_PGTABLE_DEPOSIT
void pgtable_trans_huge_deposit(struct mm_struct *mm, pmd_t *pmdp,
				pgtable_t pgtable);

#define __HAVE_ARCH_PGTABLE_WITHDRAW
pgtable_t pgtable_trans_huge_withdraw(struct mm_struct *mm, pmd_t *pmdp);

#define  __HAVE_ARCH_PMDP_SET_ACCESS_FLAGS
static inline int pmdp_set_access_flags(struct vm_area_struct *vma,
					unsigned long addr, pmd_t *pmdp,
					pmd_t entry, int dirty)
{
	VM_BUG_ON(addr & ~HPAGE_MASK);

	entry = pmd_mkyoung(entry);
	if (dirty)
		entry = pmd_mkdirty(entry);
	if (pmd_val(*pmdp) == pmd_val(entry))
		return 0;
	pmdp_xchg_direct(vma->vm_mm, addr, pmdp, entry);
	return 1;
}

#define __HAVE_ARCH_PMDP_TEST_AND_CLEAR_YOUNG
static inline int pmdp_test_and_clear_young(struct vm_area_struct *vma,
					    unsigned long addr, pmd_t *pmdp)
{
	pmd_t pmd = *pmdp;

	pmd = pmdp_xchg_direct(vma->vm_mm, addr, pmdp, pmd_mkold(pmd));
	return pmd_young(pmd);
}

#define __HAVE_ARCH_PMDP_CLEAR_YOUNG_FLUSH
static inline int pmdp_clear_flush_young(struct vm_area_struct *vma,
					 unsigned long addr, pmd_t *pmdp)
{
	VM_BUG_ON(addr & ~HPAGE_MASK);
	return pmdp_test_and_clear_young(vma, addr, pmdp);
}

static inline void set_pmd_at(struct mm_struct *mm, unsigned long addr,
			      pmd_t *pmdp, pmd_t entry)
{
	set_pmd(pmdp, entry);
}

static inline pmd_t pmd_mkhuge(pmd_t pmd)
{
	pmd = set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_LARGE));
	pmd = set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_YOUNG));
	return set_pmd_bit(pmd, __pgprot(_SEGMENT_ENTRY_PROTECT));
}

#define __HAVE_ARCH_PMDP_HUGE_GET_AND_CLEAR
static inline pmd_t pmdp_huge_get_and_clear(struct mm_struct *mm,
					    unsigned long addr, pmd_t *pmdp)
{
	return pmdp_xchg_direct(mm, addr, pmdp, __pmd(_SEGMENT_ENTRY_EMPTY));
}

#define __HAVE_ARCH_PMDP_HUGE_GET_AND_CLEAR_FULL
static inline pmd_t pmdp_huge_get_and_clear_full(struct vm_area_struct *vma,
						 unsigned long addr,
						 pmd_t *pmdp, int full)
{
	if (full) {
		pmd_t pmd = *pmdp;
		set_pmd(pmdp, __pmd(_SEGMENT_ENTRY_EMPTY));
		return pmd;
	}
	return pmdp_xchg_lazy(vma->vm_mm, addr, pmdp, __pmd(_SEGMENT_ENTRY_EMPTY));
}

#define __HAVE_ARCH_PMDP_HUGE_CLEAR_FLUSH
static inline pmd_t pmdp_huge_clear_flush(struct vm_area_struct *vma,
					  unsigned long addr, pmd_t *pmdp)
{
	return pmdp_huge_get_and_clear(vma->vm_mm, addr, pmdp);
}

#define __HAVE_ARCH_PMDP_INVALIDATE
static inline pmd_t pmdp_invalidate(struct vm_area_struct *vma,
				   unsigned long addr, pmd_t *pmdp)
{
	pmd_t pmd;

	VM_WARN_ON_ONCE(!pmd_present(*pmdp));
	pmd = __pmd(pmd_val(*pmdp) | _SEGMENT_ENTRY_INVALID);
	return pmdp_xchg_direct(vma->vm_mm, addr, pmdp, pmd);
}

#define __HAVE_ARCH_PMDP_SET_WRPROTECT
static inline void pmdp_set_wrprotect(struct mm_struct *mm,
				      unsigned long addr, pmd_t *pmdp)
{
	pmd_t pmd = *pmdp;

	if (pmd_write(pmd))
		pmd = pmdp_xchg_lazy(mm, addr, pmdp, pmd_wrprotect(pmd));
}

static inline pmd_t pmdp_collapse_flush(struct vm_area_struct *vma,
					unsigned long address,
					pmd_t *pmdp)
{
	return pmdp_huge_get_and_clear(vma->vm_mm, address, pmdp);
}
#define pmdp_collapse_flush pmdp_collapse_flush

#define pfn_pmd(pfn, pgprot)	mk_pmd_phys(((pfn) << PAGE_SHIFT), (pgprot))

static inline int pmd_trans_huge(pmd_t pmd)
{
	return pmd_leaf(pmd);
}

#define has_transparent_hugepage has_transparent_hugepage
static inline int has_transparent_hugepage(void)
{
	return cpu_has_edat1() ? 1 : 0;
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

/*
 * 64 bit swap entry format:
 * A page-table entry has some bits we have to treat in a special way.
 * Bits 54 and 63 are used to indicate the page type. Bit 53 marks the pte
 * as invalid.
 * A swap pte is indicated by bit pattern (pte & 0x201) == 0x200
 * |			  offset			|E11XX|type |S0|
 * |0000000000111111111122222222223333333333444444444455|55555|55566|66|
 * |0123456789012345678901234567890123456789012345678901|23456|78901|23|
 *
 * Bits 0-51 store the offset.
 * Bit 52 (E) is used to remember PG_anon_exclusive.
 * Bits 57-61 store the type.
 * Bit 62 (S) is used for softdirty tracking.
 * Bits 55 and 56 (X) are unused.
 */

#define __SWP_OFFSET_MASK	((1UL << 52) - 1)
#define __SWP_OFFSET_SHIFT	12
#define __SWP_TYPE_MASK		((1UL << 5) - 1)
#define __SWP_TYPE_SHIFT	2

static inline pte_t mk_swap_pte(unsigned long type, unsigned long offset)
{
	unsigned long pteval;

	pteval = _PAGE_INVALID | _PAGE_PROTECT;
	pteval |= (offset & __SWP_OFFSET_MASK) << __SWP_OFFSET_SHIFT;
	pteval |= (type & __SWP_TYPE_MASK) << __SWP_TYPE_SHIFT;
	return __pte(pteval);
}

static inline unsigned long __swp_type(swp_entry_t entry)
{
	return (entry.val >> __SWP_TYPE_SHIFT) & __SWP_TYPE_MASK;
}

static inline unsigned long __swp_offset(swp_entry_t entry)
{
	return (entry.val >> __SWP_OFFSET_SHIFT) & __SWP_OFFSET_MASK;
}

static inline swp_entry_t __swp_entry(unsigned long type, unsigned long offset)
{
	return (swp_entry_t) { pte_val(mk_swap_pte(type, offset)) };
}

#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })

/*
 * 64 bit swap entry format for REGION3 and SEGMENT table entries (RSTE)
 * Bits 59 and 63 are used to indicate the swap entry. Bit 58 marks the rste
 * as invalid.
 * A swap entry is indicated by bit pattern (rste & 0x011) == 0x010
 * |			  offset			|Xtype |11TT|S0|
 * |0000000000111111111122222222223333333333444444444455|555555|5566|66|
 * |0123456789012345678901234567890123456789012345678901|234567|8901|23|
 *
 * Bits 0-51 store the offset.
 * Bits 53-57 store the type.
 * Bit 62 (S) is used for softdirty tracking.
 * Bits 60-61 (TT) indicate the table type: 0x01 for REGION3 and 0x00 for SEGMENT.
 * Bit 52 (X) is unused.
 */

#define __SWP_OFFSET_MASK_RSTE	((1UL << 52) - 1)
#define __SWP_OFFSET_SHIFT_RSTE	12
#define __SWP_TYPE_MASK_RSTE		((1UL << 5) - 1)
#define __SWP_TYPE_SHIFT_RSTE	6

/*
 * TT bits set to 0x00 == SEGMENT. For REGION3 entries, caller must add R3
 * bits 0x01. See also __set_huge_pte_at().
 */
static inline unsigned long mk_swap_rste(unsigned long type, unsigned long offset)
{
	unsigned long rste;

	rste = _RST_ENTRY_INVALID | _RST_ENTRY_COMM;
	rste |= (offset & __SWP_OFFSET_MASK_RSTE) << __SWP_OFFSET_SHIFT_RSTE;
	rste |= (type & __SWP_TYPE_MASK_RSTE) << __SWP_TYPE_SHIFT_RSTE;
	return rste;
}

static inline unsigned long __swp_type_rste(swp_entry_t entry)
{
	return (entry.val >> __SWP_TYPE_SHIFT_RSTE) & __SWP_TYPE_MASK_RSTE;
}

static inline unsigned long __swp_offset_rste(swp_entry_t entry)
{
	return (entry.val >> __SWP_OFFSET_SHIFT_RSTE) & __SWP_OFFSET_MASK_RSTE;
}

#define __rste_to_swp_entry(rste)	((swp_entry_t) { rste })

extern int vmem_add_mapping(unsigned long start, unsigned long size);
extern void vmem_remove_mapping(unsigned long start, unsigned long size);
extern int __vmem_map_4k_page(unsigned long addr, unsigned long phys, pgprot_t prot, bool alloc);
extern int vmem_map_4k_page(unsigned long addr, unsigned long phys, pgprot_t prot);
extern void vmem_unmap_4k_page(unsigned long addr);
extern pte_t *vmem_get_alloc_pte(unsigned long addr, bool alloc);
extern int s390_enable_sie(void);
extern int s390_enable_skey(void);
extern void s390_reset_cmma(struct mm_struct *mm);

/* s390 has a private copy of get unmapped area to deal with cache synonyms */
#define HAVE_ARCH_UNMAPPED_AREA
#define HAVE_ARCH_UNMAPPED_AREA_TOPDOWN

#define pmd_pgtable(pmd) \
	((pgtable_t)__va(pmd_val(pmd) & -sizeof(pte_t)*PTRS_PER_PTE))

static inline unsigned long gmap_pgste_get_pgt_addr(unsigned long *pgt)
{
	unsigned long *pgstes, res;

	pgstes = pgt + _PAGE_ENTRIES;

	res = (pgstes[0] & PGSTE_ST2_MASK) << 16;
	res |= pgstes[1] & PGSTE_ST2_MASK;
	res |= (pgstes[2] & PGSTE_ST2_MASK) >> 16;
	res |= (pgstes[3] & PGSTE_ST2_MASK) >> 32;

	return res;
}

#endif /* _S390_PAGE_H */
