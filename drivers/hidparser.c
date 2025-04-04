/*
 * hidparser.c:	HID Parser
 *
 * This file is part of the MGE UPS SYSTEMS HID Parser
 *
 * Copyright (C)
 *	1998-2003	MGE UPS SYSTEMS, Luc Descotils
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * -------------------------------------------------------------------------- */

#include "config.h" /* must be first */

#include <string.h>
#include <stdlib.h>

#include "hidparser.h"
#include "nut_stdint.h"  /* for int8_t, int16_t, int32_t */
#include "common.h"      /* for fatalx() */

static const uint8_t ItemSize[4] = { 0, 1, 2, 4 };

/*
 * HIDParser struct
 * -------------------------------------------------------------------------- */
/* FIXME? Should this structure remain with reasonable fixed int types,
 * or changed to align with libusb API version and usb_ctrl_* typedefs?
 */
typedef struct {
	const unsigned char	*ReportDesc;		/* Report Descriptor		*/
	size_t			ReportDescSize;		/* Size of Report Descriptor	*/

	uint16_t	Pos;				/* Store current pos in descriptor	*/
	uint8_t		Item;				/* Store current Item		*/
	uint32_t	Value;				/* Store current Value		*/

	HIDData_t	Data;				/* Store current environment	*/

	uint8_t		OffsetTab[MAX_REPORT][4];	/* Store ID, Type, offset & timestamp of report */
	uint8_t		ReportCount;			/* Store Report Count		*/
	uint8_t		Count;				/* Store local report count	*/

	uint16_t	UPage;				/* Global UPage			*/
	HIDNode_t	UsageTab[USAGE_TAB_SIZE];	/* Usage stack			*/
	uint8_t		UsageSize;			/* Design number of usage used	*/
} HIDParser_t;

/* return 1 + the position of the leftmost "1" bit of an int, or 0 if
   none. */
static inline unsigned int hibit(unsigned long x)
{
	unsigned int	res = 0;

	while (x > 0xff) {
		x >>= 8;
		res += 8;
	}

	while (x) {
		x >>= 1;
		res += 1;
	}

	return res;
}

/* Note: The USB HID specification states that Local items do not
   carry over to the next Main item (version 1.11, section
   6.2.2.8). Therefore the local state must be reset after each main
   item. In particular, any unused usages on the Usage tabs must be
   discarded and must not carry over to the next Main item. Some APC
   equipment actually sends multiple redundant "usage" commands for a
   single control, so resetting the local state is important. */
/* Also note: UsageTab[0] is used as the usage of the next control,
   even if UsageSize=0. Therefore, this must be initialized */
static void ResetLocalState(HIDParser_t* pParser)
{
	pParser->UsageSize = 0;
	memset(pParser->UsageTab, 0, sizeof(pParser->UsageTab));
}

/*
 * GetReportOffset
 *
 * Return pointer on current offset value for Report designed by
 * ReportID/ReportType
 * -------------------------------------------------------------------------- */
static uint8_t *GetReportOffset(HIDParser_t* pParser, const uint8_t ReportID, const uint8_t ReportType)
{
	int	Pos;

	for (Pos = 0; Pos < MAX_REPORT; Pos++) {

		if (pParser->OffsetTab[Pos][0] == 0) {
			pParser->OffsetTab[Pos][0] = ReportID;
			pParser->OffsetTab[Pos][1] = ReportType;
			pParser->OffsetTab[Pos][2] = 0;
		}

		if (pParser->OffsetTab[Pos][0] != ReportID) {
			continue;
		}

		if (pParser->OffsetTab[Pos][1] != ReportType) {
			continue;
		}

		return &pParser->OffsetTab[Pos][2];
	}

	return NULL;
}

/*
 * FormatValue(uint32_t Value, uint8_t Size)
 * Format Value to fit with long format with respect of negative values
 * -------------------------------------------------------------------------- */
static long FormatValue(uint32_t Value, uint8_t Size)
{
	switch(Size)
	{
	case 1:
		return (long)(int8_t)Value;
	case 2:
		return (long)(int16_t)Value;
	case 4:
		return (long)(int32_t)Value;
	default:
		return (long)Value;
	}
}

/*
 * HIDParse(HIDParser_t* pParser, HIDData_t *pData)
 *
 * Analyse Report descriptor stored in HIDParser struct and store local and
 * global context.
 * Return in pData the last object found.
 * Return -1 when there is no other Item to parse, 1 if a new object was found
 * or 0 if a continuation of a previous object was found.
 * -------------------------------------------------------------------------- */
static int HIDParse(HIDParser_t *pParser, HIDData_t *pData)
{
	int	Found = -1, i;

	while ((Found < 0) && (pParser->Pos < pParser->ReportDescSize)) {
		/* Get new pParser->Item if current pParser->Count is empty */
		if (pParser->Count == 0) {
			pParser->Item = pParser->ReportDesc[pParser->Pos++];
			pParser->Value = 0;
			for (i = 0; i < ItemSize[pParser->Item & SIZE_MASK]; i++) {
				pParser->Value += (uint32_t)(pParser->ReportDesc[(pParser->Pos)+i]) << (8*i);
			}
			/* Pos on next item */
			pParser->Pos += ItemSize[pParser->Item & SIZE_MASK];
		}

		switch (pParser->Item & ITEM_MASK)
		{
		case ITEM_UPAGE:
			/* Copy UPage in Usage stack */
			pParser->UPage=(uint16_t)pParser->Value;
			break;

		case ITEM_USAGE:
			/* Copy global or local UPage if any, in Usage stack */
			if ((pParser->Item & SIZE_MASK) > 2) {
				pParser->UsageTab[pParser->UsageSize] = pParser->Value;
			} else {
				pParser->UsageTab[pParser->UsageSize] = ((HIDNode_t)(pParser->UPage) << 16) | (pParser->Value & 0xFFFF);
			}

			/* Increment Usage stack size */
			pParser->UsageSize++;
			break;

		case ITEM_COLLECTION:
			/* Get UPage/Usage from UsageTab and store them in pParser->Data.Path */
			pParser->Data.Path.Node[pParser->Data.Path.Size] = pParser->UsageTab[0];
			pParser->Data.Path.Size++;

			/* Unstack UPage/Usage from UsageTab (never remove the last) */
			if (pParser->UsageSize > 0) {
				int	j;

				for (j = 0; j < pParser->UsageSize; j++) {
					pParser->UsageTab[j] = pParser->UsageTab[j+1];
				}

				/* Remove Usage */
				pParser->UsageSize--;
			}

			/* Get Index if any */
			if (pParser->Value >= 0x80) {
				pParser->Data.Path.Node[pParser->Data.Path.Size] = 0x00ff0000 | (pParser->Value & 0x7F);
				pParser->Data.Path.Size++;
			}

			ResetLocalState(pParser);
			break;

		case ITEM_END_COLLECTION :
			pParser->Data.Path.Size--;

			/* Remove Index if any */
			if((pParser->Data.Path.Node[pParser->Data.Path.Size] & 0xffff0000) == 0x00ff0000) {
				pParser->Data.Path.Size--;
			}

			ResetLocalState(pParser);
			break;

		case ITEM_FEATURE:
		case ITEM_INPUT:
		case ITEM_OUTPUT:
			if (pParser->UsageTab[0] != 0x00000000) {
				/* An object was found if the path does not end with 0x00000000 */
				Found = 1;
			} else {
				/* It is a continuation of a previous object */
				Found = 0;
			}

			/* Get new pParser->Count from global value */
			if(pParser->Count == 0) {
				pParser->Count = pParser->ReportCount;
			}

			/* Get UPage/Usage from UsageTab and store them in pParser->Data.Path */
			pParser->Data.Path.Node[pParser->Data.Path.Size] = pParser->UsageTab[0];
			pParser->Data.Path.Size++;

			/* Unstack UPage/Usage from UsageTab (never remove the last) */
			if(pParser->UsageSize > 0) {
				int j;

				for (j = 0; j < pParser->UsageSize; j++) {
					pParser->UsageTab[j] = pParser->UsageTab[j+1];
				}

				/* Remove Usage */
				pParser->UsageSize--;
			}

			/* Copy data type */
			pParser->Data.Type = (uint8_t)(pParser->Item & ITEM_MASK);

			/* Copy data attribute */
			pParser->Data.Attribute = (uint8_t)pParser->Value;

			/* Store offset */
			pParser->Data.Offset = *GetReportOffset(pParser, pParser->Data.ReportID, (uint8_t)(pParser->Item & ITEM_MASK));

			/* Get Object in pData */
			/* -------------------------------------------------------------------------- */
			memcpy(pData, &pParser->Data, sizeof(HIDData_t));
			/* -------------------------------------------------------------------------- */

			/* Increment Report Offset */
			*GetReportOffset(pParser, pParser->Data.ReportID, (uint8_t)(pParser->Item & ITEM_MASK)) += pParser->Data.Size;

			/* Remove path last node */
			pParser->Data.Path.Size--;

			/* Decrement count */
			pParser->Count--;

			if (pParser->Count == 0) {
				ResetLocalState(pParser);
			}
			break;

		case ITEM_REP_ID :
			pParser->Data.ReportID = (uint8_t)pParser->Value;
			break;

		case ITEM_REP_SIZE :
			pParser->Data.Size = (uint8_t)pParser->Value;
			break;

		case ITEM_REP_COUNT :
			pParser->ReportCount = (uint8_t)pParser->Value;
			break;

		case ITEM_UNIT_EXP :
			pParser->Data.UnitExp = (int8_t)pParser->Value;
			if (pParser->Data.UnitExp > 7) {
				pParser->Data.UnitExp |= 0xF0;
			}
			break;

		case ITEM_UNIT :
			/* TOTHINK: Are there cases where Unit is not-signed,
			 * but a Value too big becomes signed after casting --
			 * and unintentionally so? */
			pParser->Data.Unit = (long)pParser->Value;
			break;

		case ITEM_LOG_MIN :
			pParser->Data.LogMin = FormatValue(pParser->Value, ItemSize[pParser->Item & SIZE_MASK]);
			break;

		case ITEM_LOG_MAX :
			pParser->Data.LogMax = FormatValue(pParser->Value, ItemSize[pParser->Item & SIZE_MASK]);
			/* HACK: If treating the value as signed (FormatValue(...))
			 *  results in a LogMax that is less than the LogMin
			 *  value then it is likely that the LogMax value has
			 *  been incorrectly encoded by the UPS firmware
			 *  (field was too short and overflowed into sign bit).
			 * In that case, reinterpret it as an unsigned number
			 *  and log the problem. See also *_fix_report_desc()
			 *  methods that follow up in some *-hid.c subdrivers.
			 * This hack is not correct in the sense that it only
			 *  looks at the LogMin value for this item, whereas
			 *  the HID spec says that Logical values persist in
			 *  global state.
			 * Note the values MAY be signed or unsigned, according
			 *  to rules and circumstances explored below.
			 *
			 * RATIONALE: per discussions such as:
			 * * https://github.com/networkupstools/nut/issues/1512#issuecomment-1238310056
			 *   The encoding of small integers in the logical/physical
			 *    min/max fields (in fact, I think in anywhere they
			 *    encode integers) is independent of the size of
			 *    the (feature) field they end up referring to.
			 *   One should use the smallest size encoding
			 *    (0, 1, 2, or 4 bytes are the options) that can
			 *    represent, as a signed quantity, the value you
			 *    need to encode. See HID spec 1.11, sec 6.2.2.2
			 *    "Short Items". Given a 16 bit report field, with
			 *    logical values 0..65535, it should use a 0 byte
			 *    encoding for the logical minimum (0x14, rather
			 *    than 0x15 0x00) and a 4-byte encoding for the
			 *    logical maximum (0x27 0xFF 0xFF 0x00 0x00).
			 *   Their encoding choice does suggest you cannot
			 *    have an unsigned 32-bit report item with logical
			 *    maximum >2147483647 (unless you assume, as I did,
			 *    that "if max < min" then it's just a bad encoding
			 *    of a positive number that ran into the sign bit).
			 * * https://github.com/networkupstools/nut/pull/2718#issuecomment-2547021458
			 *   This is what the spec says (page labelled 19 of
			 *   hid1_11.pdf, physical page 29 of 97) --
			 *   5.8 Format of Multibyte Numeric Values
			 *    Multibyte numeric values in reports are
			 *    represented in little-endian format, with the
			 *    least significant byte at the lowest address.
			 *    The Logical Minimum and Logical Maximum values
			 *    identify the range of values that will be found
			 *    in a report.
			 *    If Logical Minimum and Logical Maximum are
			 *    both positive values then a sign bit is
			 *    unnecessary in the report field and the
			 *    contents of a field can be assumed to
			 *    be an unsigned value.
			 *    Otherwise, all integer values are signed
			 *    values represented in 2's complement format.
			 *    Floating point values are not allowed.
			 * * https://github.com/networkupstools/nut/pull/2718#issuecomment-2547065141
			 *    The number of bytes in the encoding of the
			 *     LogMin and LogMax fields is only loosely tied
			 *     to the "Size" of the field that they are
			 *     describing -- but the implementers on the
			 *     UPS side don't seem to quite get that.
			 *    It's all starting to come back to me...
			 *    If you're trying to describe a report field
			 *     that is 16-bits and has (unsigned) values
			 *     from 0..65535 range, then you SHOULD have
			 *     a LogMin field containing value 0, and
			 *     a LogMax field that contains value 65535.
			 *    Since all numeric fields are interpreted as
			 *     signed "two's-complement" values (except for
			 *     that note above about the *report values*,
			 *     NOT the values in the report description), to
			 *     encode such a LogMax field you would have to
			 *     express the *LogMax field* in a 4-byte encoding
			 *     in the *report description*.
			 *    That's independent of the ultimate 2-byte
			 *     *report value* that these LogMin and LogMax
			 *     are describing.
			 *    We suppose that some coder at the UPS company
			 *     took a shortcut, and set not only "LogMin = 0",
			 *     but also effectively "LogMax = -1" (because they
			 *     used a 2-byte encoding with all bits set, not a
			 *     4-byte encoding), and then NUT is left to decide
			 *     what they actually intended.
			 *    My interpretation of that is that they're trying
			 *     to say e.g. 0..65535, because if they had meant
			 *     0..32767 they would have just written that (as
			 *     0..7FFF which fits in the signed 2-byte repr.),
			 *     but unless they're actually trying to represent
			 *     e.g. voltages over 327 V, deciding that the
			 *     limit is a signed 32767 should also be fine.
			 *
			 * ...and maybe some in other tickets
			 *
			 * TL;DR: there is likely a mis-understanding
			 *  of the USB spec by firmware developers.
			 */
			if (pParser->Data.LogMax < pParser->Data.LogMin) {
				upslogx(LOG_WARNING,
					"%s: LogMax is less than LogMin. "
					"Vendor HID report descriptor may be incorrect; "
					"interpreting LogMax %ld as %u in ReportID: 0x%02x",
					__func__,
					pParser->Data.LogMax,
					pParser->Value,
					pParser->Data.ReportID);
				pParser->Data.assumed_LogMax = true;
				pParser->Data.LogMax = (long) pParser->Value;
			}
			break;

		case ITEM_PHY_MIN :
			pParser->Data.PhyMin=FormatValue(pParser->Value, ItemSize[pParser->Item & SIZE_MASK]);
			pParser->Data.have_PhyMin = 1;
			break;

		case ITEM_PHY_MAX :
			pParser->Data.PhyMax=FormatValue(pParser->Value, ItemSize[pParser->Item & SIZE_MASK]);
			pParser->Data.have_PhyMax = 1;
			break;

		case ITEM_LONG :
			/* can't handle long items, but should at least skip them */
			pParser->Pos += (uint8_t)(pParser->Value & 0xff);
			break;

		default:
			break;
		}
	} /* while ((Found < 0) && (pParser->Pos < pParser->ReportDescSize)) */

	if(pParser->Data.Path.Size >= PATH_SIZE)
		upslogx(LOG_ERR, "%s: HID path too long", __func__);
	if(pParser->ReportDescSize >= REPORT_DSC_SIZE)
		upslogx(LOG_ERR, "%s: Report descriptor too big", __func__);
	if(pParser->UsageSize >= USAGE_TAB_SIZE)
		upslogx(LOG_ERR, "%s: HID Usage too high", __func__);

	/* FIXME: comparison is always false due to limited range of data type [-Werror=type-limits]
	 * with ReportID being uint8_t and MAX_REPORT being 500 currently */
	/*
	if(pParser->Data.ReportID >= MAX_REPORT)
		upslogx(LOG_ERR, "%s: Too many HID reports", __func__);
	*/

	return Found;
}

/*
 * FindObject
 * Get pData characteristics from pData->Path
 * Return TRUE if object was found
 * -------------------------------------------------------------------------- */
int FindObject(HIDDesc_t *pDesc_arg, HIDData_t *pData)
{
	HIDData_t	*pFoundData = FindObject_with_Path(pDesc_arg, &pData->Path, pData->Type);

	if (!pFoundData) {
		return 0;
	}

	memcpy(pData, pFoundData, sizeof(*pData));
	return 1;
}

/*
 * FindObject_with_Path
 * Get pData item with given Path and Type. Return NULL if not found.
 * -------------------------------------------------------------------------- */
HIDData_t *FindObject_with_Path(HIDDesc_t *pDesc_arg, HIDPath_t *Path, uint8_t Type)
{
	size_t	i;

	for (i = 0; i < pDesc_arg->nitems; i++) {
		HIDData_t *pData = &pDesc_arg->item[i];

		if (pData->Type != Type) {
			continue;
		}

		if (memcmp(pData->Path.Node, Path->Node, (Path->Size) * sizeof(HIDNode_t))) {
			continue;
		}

		return pData;
	}

	return NULL;
}

/*
 * FindObject_with_ID
 * Get pData item with given ReportID, Offset, and Type. Return NULL
 * if not found.
 * -------------------------------------------------------------------------- */
HIDData_t *FindObject_with_ID(HIDDesc_t *pDesc_arg, uint8_t ReportID, uint8_t Offset, uint8_t Type)
{
	size_t	i;

	for (i = 0; i < pDesc_arg->nitems; i++) {
		HIDData_t *pData = &pDesc_arg->item[i];

		if (pData->ReportID != ReportID) {
			continue;
		}

		if (pData->Type != Type) {
			continue;
		}

		if (pData->Offset != Offset) {
			continue;
		}

		return pData;
	}

	return NULL;
}

/*
 * FindObject_with_ID_Node
 * Get pData item with given ReportID and Node. Return NULL if not found.
 * -------------------------------------------------------------------------- */
HIDData_t *FindObject_with_ID_Node(HIDDesc_t *pDesc_arg, uint8_t ReportID, HIDNode_t Node)
{
	size_t	i;

	for (i = 0; i < pDesc_arg->nitems; i++) {
		HIDData_t	*pData = &pDesc_arg->item[i];
		HIDPath_t	*pPath;
		uint8_t	size;

		if (pData->ReportID != ReportID) {
			continue;
		}

		pPath = &pData->Path;
		size = pPath->Size;
		if (size == 0 || pPath->Node[size-1] != Node) {
			continue;
		}

		return pData;
	}

	return NULL;
}

/*
 * GetValue
 * Extract data from a report stored in Buf.
 * Use Offset, Size, LogMin, and LogMax of pData.
 * Return response in *pValue.
 * -------------------------------------------------------------------------- */
void GetValue(const unsigned char *Buf, HIDData_t *pData, long *pValue)
{
	/* Note:  https://github.com/networkupstools/nut/issues/1023
	   This conversion code can easily be sensitive to 32- vs. 64- bit
	   compilation environments.  Consider the possibility of overflow
	   in 32-bit representations when computing with extreme values,
	   for example LogMax-LogMin+1.
	   Test carefully in both environments if changing any declarations.
	*/

	int	Weight, Bit;
	unsigned long mask, signbit, magMax, magMin;
	long	value = 0;

	Bit = pData->Offset + 8;	/* First byte of report is report ID */

	for (Weight = 0; Weight < pData->Size; Weight++, Bit++) {
		int	State = Buf[Bit >> 3] & (1 << (Bit & 7));

		if(State) {
			value += (1L << Weight);
		}
	}

	/* translate Value into a signed/unsigned value in the range
	LogMin..LogMax, as appropriate. See HID spec, p.38: "If both the
	Logical Minimum and Logical Maximum extents are defined as
	positive values (0 or greater), then the report field can be
	assumed to be an unsigned value. Otherwise, all integer values
	are signed values represented in 2's complement format."

	Also note that the variable can take values from LogMin
	(inclusive) to LogMax (inclusive), so there are LogMax - LogMin +
	1 possible values.

	Special cases arise if the value that has been read lies outside
	the interval LogMin..LogMax. Some devices, notably the APC
	Back-UPS BF500, do this. In one case I observed, LogMin=0,
	LogMax=0xffff, Size=32, and the supplied value is
	0xffffffff80080a00. Presumably they expect us to throw away the
	higher-order bits, and use 0x0a00, rather than choosing the
	closest value in the interval, which would be 0xffff.  However,
	if LogMax - LogMin + 1 isn't a power of 2, it is not clear what
	"throwing away higher-order bits" exacly means, so we try to do
	something sensible. -PS */

	/* determine representation without sign bit */
	magMax = pData->LogMax >= 0 ? (unsigned long)(pData->LogMax) : (unsigned long)(-(pData->LogMax + 1));
	magMin = pData->LogMin >= 0 ? (unsigned long)(pData->LogMin) : (unsigned long)(-(pData->LogMin + 1));

	/* calculate where the sign bit will be if needed */
	signbit = 1L << hibit(magMax > magMin ? magMax : magMin);

	/* but only include sign bit in mask if negative numbers are involved */
	mask = (signbit - 1) | ((pData->LogMin < 0) ? signbit : 0);

	/* throw away excess high order bits (which may contain garbage) */
	value = (long)((unsigned long)(value) & mask);

	/* sign-extend it, if appropriate */
	if (pData->LogMin < 0 && ((unsigned long)(value) & signbit) != 0) {
		value |= ~mask;
	}

	/* clamp returned value to range [LogMin..LogMax] */
	if (value < pData->LogMin) {
		value = pData->LogMin;
	} else if (value > pData->LogMax) {
		value = pData->LogMax;
	}

	*pValue = value;
	return;
}

/*
 * SetValue
 * Set a data in a report stored in Buf. Use Value, Offset and Size of pData.
 * Return response in Buf.
 * -------------------------------------------------------------------------- */
void SetValue(const HIDData_t *pData, unsigned char *Buf, long Value)
{
	int	Weight, Bit;

	Bit = pData->Offset + 8;	/* First byte of report is report ID */

	for (Weight = 0; Weight < pData->Size; Weight++, Bit++) {
		long	State = Value & (1L << Weight);

		if (State) {
			Buf[Bit >> 3] |= (1 << (Bit & 7));
		} else {
			Buf[Bit >> 3] &= ~(1 << (Bit & 7));
		}
	}
}

/* ---------------------------------------------------------------------- */

/* parse HID Report Descriptor. Input: byte array ReportDesc[n].
   Output: parsed data structure. Returns allocated HIDDesc structure
   on success, NULL on failure with errno set. Note: the value
   returned by this function must be freed with Free_ReportDesc(). */
HIDDesc_t *Parse_ReportDesc(const usb_ctrl_charbuf ReportDesc, const usb_ctrl_charbufsize n)
{
	int		ret = 0;
	HIDDesc_t	*pDesc_var;
	HIDParser_t	*parser;

	pDesc_var = calloc(1, sizeof(*pDesc_var));
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
	if (!pDesc_var
	|| n < 0 || (uintmax_t)n > SIZE_MAX
	) {
		return NULL;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif

	pDesc_var->item = calloc(MAX_REPORT, sizeof(*pDesc_var->item));
	if (!pDesc_var->item) {
		Free_ReportDesc(pDesc_var);
		return NULL;
	}

	parser = calloc(1, sizeof(*parser));
	if (!parser) {
		Free_ReportDesc(pDesc_var);
		return NULL;
	}

	parser->ReportDesc = (const unsigned char *)ReportDesc;
	parser->ReportDescSize = (const size_t)n;

	for (pDesc_var->nitems = 0; pDesc_var->nitems < MAX_REPORT; pDesc_var->nitems += (size_t)ret) {
		uint8_t	id;
		size_t	max;

		ret = HIDParse(parser, &pDesc_var->item[pDesc_var->nitems]);
		if (ret < 0) {
			break;
		}

		id = pDesc_var->item[pDesc_var->nitems].ReportID;

		/* calculate bit range of this item within report */
		max = pDesc_var->item[pDesc_var->nitems].Offset + pDesc_var->item[pDesc_var->nitems].Size;

		/* convert to bytes */
		max = (max + 7) >> 3;

		/* update report length */
		if (max > pDesc_var->replen[id]) {
			pDesc_var->replen[id] = max;
		}
	}

	/* Sanity check: are there remaining HID objects that can't
	 * be processed? */
	if ((pDesc_var->nitems == MAX_REPORT) && (parser->Pos < parser->ReportDescSize))
		upslogx(LOG_ERR, "ERROR in %s: Too many HID objects", __func__);

	free(parser);

	if (pDesc_var->nitems == 0) {
		Free_ReportDesc(pDesc_var);
		return NULL;
	}

	pDesc_var->item = realloc(pDesc_var->item, pDesc_var->nitems * sizeof(*pDesc_var->item));

	return pDesc_var;
}

/* free a parsed report descriptor, as allocated by Parse_ReportDesc() */
void Free_ReportDesc(HIDDesc_t *pDesc_arg)
{
	if (!pDesc_arg) {
		return;
	}

	free(pDesc_arg->item);
	free(pDesc_arg);
}
