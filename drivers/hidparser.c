/*
 * hidparser.c: HID Parser
 *
 * This file is part of the MGE UPS SYSTEMS HID Parser.
 *
 * Copyright (C) 1998-2003 MGE UPS SYSTEMS,
 *		Written by Luc Descotils.
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

#include <string.h>
#include "config.h"
#include "hidparser.h"

/* to be implemented for DEBUG purpose */
/* previously: #define ERROR(x) if(x) __asm { int 3 }; */
#define ERROR(x)

static const char ItemSize[4]={0,1,2,4};

/*
 * ResetParser
 * Reset HIDParser structure for new parsing
 * Keep Report descriptor data
 * -------------------------------------------------------------------------- */
void ResetParser(HIDParser* pParser)
{
  pParser->Pos=0;
  pParser->Count=0;
  pParser->nObject=0;
  pParser->nReport=0;

  pParser->UsageSize=0;
  memset(pParser->UsageTab,0,sizeof(pParser->UsageTab));

  memset(pParser->OffsetTab,0,sizeof(pParser->OffsetTab));
  memset(&pParser->Data,0,sizeof(pParser->Data));
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
static void ResetLocalState(HIDParser* pParser)
{
  pParser->UsageSize = 0;
  memset(pParser->UsageTab,0,sizeof(pParser->UsageTab));
}

/*
 * GetReportOffset
 *
 * Return pointer on current offset value for Report designed by 
 * ReportID/ReportType
 * -------------------------------------------------------------------------- */
static u_char* GetReportOffset(HIDParser* pParser, 
                       const u_char ReportID, 
                       const u_char ReportType)
{
  u_short Pos=0;
  while(Pos<MAX_REPORT && pParser->OffsetTab[Pos][0]!=0)
  {
    if(pParser->OffsetTab[Pos][0]==ReportID 
      && pParser->OffsetTab[Pos][1]==ReportType)
      return &pParser->OffsetTab[Pos][2];
    Pos++;
  }
  if(Pos<MAX_REPORT)
  {
    /* Increment Report count */
    pParser->nReport++;
    pParser->OffsetTab[Pos][0]=ReportID;
    pParser->OffsetTab[Pos][1]=ReportType;
    pParser->OffsetTab[Pos][2]=0;
    return &pParser->OffsetTab[Pos][2];
  }
  return NULL;
}

/*
 * FormatValue(long Value, u_char Size)
 * Format Value to fit with long format with respect of negative values
 * -------------------------------------------------------------------------- */
static long FormatValue(long Value, u_char Size)
{
  if(Size==1) 
    Value=(long)(char)Value;
  else if(Size==2) 
    Value=(long)(short)Value;
  return Value;
}

/*
 * HIDParse(HIDParser* pParser, HIDData* pData)
 *
 * Analyse Report descriptor stored in HIDParser struct and store local and
 * global context. 
 * Use ResetParser() to init HIDParser structure before beginning.
 * Return in pData the last object found.
 * Return TRUE when there is other Item to parse.
 * -------------------------------------------------------------------------- */
int HIDParse(HIDParser* pParser, HIDData* pData)
{
  int Found=0;

  while(!Found && pParser->Pos<pParser->ReportDescSize)
  {
    /* Get new pParser->Item if current pParser->Count is empty */
    if(pParser->Count==0)
    {
      pParser->Item=pParser->ReportDesc[pParser->Pos++];
      pParser->Value=0;
#if WORDS_BIGENDIAN
	{
	  int i;
	  unsigned long valTmp=0;
	  for (i=0;i<ItemSize[pParser->Item & SIZE_MASK];i++)
	    {
		memcpy(&valTmp, &pParser->ReportDesc[(pParser->Pos)+i], 1);
		pParser->Value+=valTmp>>((3-i)*8);
		valTmp=0;
	    }
	}
#else
      memcpy(&pParser->Value, &pParser->ReportDesc[pParser->Pos], ItemSize[pParser->Item & SIZE_MASK]);
#endif
      /* Pos on next item */
      pParser->Pos+=ItemSize[pParser->Item & SIZE_MASK];
    }

    switch(pParser->Item & ITEM_MASK)
    {
      case ITEM_UPAGE :
      {
        /* Copy UPage in Usage stack */
        pParser->UPage=(u_short)pParser->Value;
        break;
      }
      case ITEM_USAGE :
      {
        /* Copy global or local UPage if any, in Usage stack */
         if((pParser->Item & SIZE_MASK)>2)
          pParser->UsageTab[pParser->UsageSize].UPage=(u_short)(pParser->Value>>16);
        else
          pParser->UsageTab[pParser->UsageSize].UPage=pParser->UPage;

        /* Copy Usage in Usage stack */
        pParser->UsageTab[pParser->UsageSize].Usage=(u_short)(pParser->Value & 0xFFFF);

        /* Increment Usage stack size */
        pParser->UsageSize++;

        break;
      }
      case ITEM_COLLECTION :
      {
        /* Get UPage/Usage from UsageTab and store them in pParser->Data.Path */
        pParser->Data.Path.Node[pParser->Data.Path.Size].UPage=pParser->UsageTab[0].UPage;
        pParser->Data.Path.Node[pParser->Data.Path.Size].Usage=pParser->UsageTab[0].Usage;
        pParser->Data.Path.Size++;
      
        /* Unstack UPage/Usage from UsageTab (never remove the last) */
        if(pParser->UsageSize>0)
        {
          u_char ii=0;
          while(ii<pParser->UsageSize)
          {
            pParser->UsageTab[ii].Usage=pParser->UsageTab[ii+1].Usage;
            pParser->UsageTab[ii].UPage=pParser->UsageTab[ii+1].UPage;
            ii++;
          }
          /* Remove Usage */
          pParser->UsageSize--;
        }

        /* Get Index if any */
        if(pParser->Value>=0x80)
        {
          pParser->Data.Path.Node[pParser->Data.Path.Size].UPage=0xFF;
          pParser->Data.Path.Node[pParser->Data.Path.Size].Usage=pParser->Value & 0x7F;
          pParser->Data.Path.Size++;
        }
	ResetLocalState(pParser);
        break;
      }
      case ITEM_END_COLLECTION :
      {
        pParser->Data.Path.Size--;
        /* Remove Index if any */
        if(pParser->Data.Path.Node[pParser->Data.Path.Size].UPage==0xFF)
          pParser->Data.Path.Size--;
	ResetLocalState(pParser);
        break;
      }
      case ITEM_FEATURE :
      case ITEM_INPUT :
      case ITEM_OUTPUT :
      {
        /* An object was found */
        Found=1;

        /* Increment object count */
        pParser->nObject++;

        /* Get new pParser->Count from global value */
        if(pParser->Count==0)
        {
          pParser->Count=pParser->ReportCount;
        }

        /* Get UPage/Usage from UsageTab and store them in pParser->Data.Path */
        pParser->Data.Path.Node[pParser->Data.Path.Size].UPage=pParser->UsageTab[0].UPage;
        pParser->Data.Path.Node[pParser->Data.Path.Size].Usage=pParser->UsageTab[0].Usage;
        pParser->Data.Path.Size++;
    
        /* Unstack UPage/Usage from UsageTab (never remove the last) */
        if(pParser->UsageSize>0)
        {
          u_char ii=0;
          while(ii<pParser->UsageSize)
          {
            pParser->UsageTab[ii].UPage=pParser->UsageTab[ii+1].UPage;
            pParser->UsageTab[ii].Usage=pParser->UsageTab[ii+1].Usage;
            ii++;
          }
          /* Remove Usage */
          pParser->UsageSize--;
        }

        /* Copy data type */
        pParser->Data.Type=(u_char)(pParser->Item & ITEM_MASK);

        /* Copy data attribute */
        pParser->Data.Attribute=(u_char)pParser->Value;

        /* Store offset */
        pParser->Data.Offset=*GetReportOffset(pParser, pParser->Data.ReportID, (u_char)(pParser->Item & ITEM_MASK));
    
        /* Get Object in pData */
        /* -------------------------------------------------------------------------- */
        memcpy(pData, &pParser->Data, sizeof(HIDData));
        /* -------------------------------------------------------------------------- */

        /* Increment Report Offset */
        *GetReportOffset(pParser, pParser->Data.ReportID, (u_char)(pParser->Item & ITEM_MASK)) += pParser->Data.Size;

        /* Remove path last node */
        pParser->Data.Path.Size--;

        /* Decrement count */
        pParser->Count--;
	if (pParser->Count == 0) {
	  ResetLocalState(pParser);
	}
        break;
      }
      case ITEM_REP_ID :
      {
        pParser->Data.ReportID=(u_char)pParser->Value;
        break;
      }
      case ITEM_REP_SIZE :
      {
        pParser->Data.Size=(u_char)pParser->Value;
        break;
      }
      case ITEM_REP_COUNT :
      {
        pParser->ReportCount=(u_char)pParser->Value;
        break;
      }
      case ITEM_UNIT_EXP :
      {
        pParser->Data.UnitExp=(char)pParser->Value;
	if (pParser->Data.UnitExp > 7)
	  pParser->Data.UnitExp|=0xF0;
        break;
      }
      case ITEM_UNIT :
      {
        pParser->Data.Unit=pParser->Value;
        break;
      }
      case ITEM_LOG_MIN :
      {
        pParser->Data.LogMin=FormatValue(pParser->Value, ItemSize[pParser->Item & SIZE_MASK]);
        break;
      }
      case ITEM_LOG_MAX :
      {
        pParser->Data.LogMax=FormatValue(pParser->Value, ItemSize[pParser->Item & SIZE_MASK]);
        break;
      }
      case ITEM_PHY_MIN :
      {
        pParser->Data.PhyMin=FormatValue(pParser->Value, ItemSize[pParser->Item & SIZE_MASK]);
        break;
      }
      case ITEM_PHY_MAX :
      {
        pParser->Data.PhyMax=FormatValue(pParser->Value, ItemSize[pParser->Item & SIZE_MASK]);
        break;
      }
      case ITEM_LONG :
      {
	/* can't handle long items, but should at least skip them */
	pParser->Pos+=(u_char)(pParser->Value & 0xff);
      }
    }
  } /* while(!Found && pParser->Pos<pParser->ReportDescSize) */

  ERROR(pParser->Data.Path.Size>=PATH_SIZE);
  ERROR(pParser->ReportDescSize>=REPORT_DSC_SIZE);
  ERROR(pParser->UsageSize>=USAGE_TAB_SIZE);
  ERROR(pParser->Data.ReportID>=MAX_REPORT);

  return Found;
}

/*
 * FindObject
 * Get pData characteristics from pData->Path or from pData->ReportID/Offset
 * Return TRUE if object was found
 * -------------------------------------------------------------------------- */
int FindObject(HIDParser* pParser, HIDData* pData)
{
  HIDData FoundData;
  ResetParser(pParser);
  while(HIDParse(pParser, &FoundData))
  {
    if(pData->Path.Size>0 && 
      FoundData.Type==pData->Type &&
      memcmp(FoundData.Path.Node, pData->Path.Node, (pData->Path.Size)*sizeof(HIDNode))==0)
    {
      memcpy(pData, &FoundData, sizeof(HIDData));
      return 1;
    }
    /* Found by ReportID/Offset */
    else if(FoundData.ReportID==pData->ReportID && 
      FoundData.Type==pData->Type &&
      FoundData.Offset==pData->Offset)
    {
      memcpy(pData, &FoundData, sizeof(HIDData));
      return 1;
    }
  }
  return 0;
}

/*
 * GetValue
 * Extract data from a report stored in Buf.
 * Use Value, Offset, Size and LogMax of pData.
 * Return response in Value.
 * -------------------------------------------------------------------------- */
void GetValue(const u_char* Buf, HIDData* pData)
{
  int Bit=pData->Offset+8; /* First byte of report indicate report ID */
  int Weight=0;
  pData->Value=0;

  while(Weight<pData->Size)
  {
    int State=Buf[Bit>>3]&(1<<(Bit%8));
    if(State)
    {
      pData->Value+=(1<<Weight);
    }
    Weight++;
    Bit++;
  }
/*  if(pData->Value > pData->LogMax)
    pData->Value=FormatValue(pData->Value, (u_char)((pData->Size-1)/8+1));
*/
  if (pData->Value > pData->LogMax)
    pData->Value |= ~pData->LogMax;
}

/*
 * SetValue
 * Set a data in a report stored in Buf. Use Value, Offset and Size of pData.
 * Return response in Buf.
 * -------------------------------------------------------------------------- */
void SetValue(const HIDData* pData, u_char* Buf)
{
  int Bit=pData->Offset+8; /* First byte of report indicate report ID */
  int Weight=0;

  while(Weight<pData->Size)
  {
    int State=pData->Value & (1<<Weight);
    
    if(Bit%8==0)
      Buf[Bit/8]=0;

    if(State)
    {
      Buf[Bit/8]+=(1<<(Weight%8));
    }
    Weight++;
    Bit++;
  }
}
