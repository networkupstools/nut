/* newhidups.c - New prototype HID UPS driver for Network UPS Tools
 * 
 * Copyright (C) 2003-2004
 * Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "main.h"
#include "libhid.h"
#include "newhidups.h"

/* include all known hid2nut lookup tables */
#include "mgehid.h"

/* pointer to the good HidNut lookup table */
static hid_info_t *hid_ups;

/* Global vars */
static HIDDevice *hd;
static int offdelay = DEFAULT_OFFDELAY;
static int ondelay = DEFAULT_ONDELAY;
MatchFlags flg;
static bool data_has_changed = FALSE; /* for SEMI_STATIC data polling */
  
/* support functions */
static int setvar(const char *varname, const char *val);
static hid_info_t *find_info(const char *varname);
static char *hu_find_infoval(info_lkp_t *hid2info, long value);
static char *format_model_name(char *iProduct, char *iModel);

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
void upsdrv_shutdown(void)
{
  char delay[6];

  /* 1) set DelayBeforeStartup */
  if (getval ("ondelay"))
    ondelay = atoi (getval ("ondelay"));

  //  HIDSetItemValue(find_info("ups.delay.start")->hidpath, ondelay);
  sprintf(&delay[0], "%i", ondelay);
  if (setvar("ups.delay.start", &delay[0])!= STAT_SET_HANDLED)
    fatalx("Shutoff command failed (setting ondelay)");

  /* 2) set DelayBeforeShutdown */
  if (getval ("offdelay"))
    offdelay = atoi (getval ("offdelay"));
  
  //HIDSetItemValue(find_info("ups.delay.shutdown")->hidpath, offdelay);
  sprintf(&delay[0], "%i", offdelay);
  if (setvar("ups.delay.shutdown", &delay[0])!= STAT_SET_HANDLED)
    fatalx("Shutoff command failed (setting offdelay)");
}

/* process instant command and take action. */
static int instcmd(const char *cmdname, const char *extradata)
{
  hid_info_t *hidups_item;

  /* 1) retrieve and check netvar & item_path */	
  hidups_item = find_info(cmdname);

  if (hidups_item == NULL || hidups_item->info_type == NULL ||
      !(hidups_item->hidflags & HU_FLAG_OK))
    {
      upsdebugx(2, "instcmd: info element unavailable %s\n", cmdname);
      /* TODO: manage items handled "manually" */
      return STAT_INSTCMD_UNKNOWN;
    }

  /* Checking if item is an instant command */
  if (!hidups_item->hidflags & HU_TYPE_CMD) {
    upsdebugx(2, "instcmd: %s is not an instant command\n", cmdname);
    return STAT_INSTCMD_UNKNOWN;
  }

  /* Actual variable setting */
  if (HIDSetItemValue(hidups_item->hidpath, atol(hidups_item->dfl))) {

    upsdebugx(3, "SUCCEED\n");
    /* Set the status so that SEMI_STATIC vars are polled */
    data_has_changed = TRUE;
    return STAT_INSTCMD_HANDLED;
  }
  else
    upsdebugx(3, "FAILED\n"); /* TODO: HANDLED but FAILED, not UNKNOWN! */

  /* TODO: to be completed */
  return STAT_INSTCMD_UNKNOWN;
}

/* set r/w INFO_ element to a value. */
static int setvar(const char *varname, const char *val)
{
  hid_info_t *hidups_item;

  upsdebugx(2, "============== entering setvar(%s, %s) ==============\n", varname, val);

  /* 1) retrieve and check netvar & item_path */	
  hidups_item = find_info(varname);

  if (hidups_item == NULL || hidups_item->info_type == NULL ||
      !(hidups_item->hidflags & HU_FLAG_OK))
    {
      upsdebugx(2, "setvar: info element unavailable %s\n", varname);
      return STAT_SET_UNKNOWN;
    }

  /* Checking item writability and HID Path */
  if (!hidups_item->info_flags & ST_FLAG_RW) {
    upsdebugx(2, "setvar: not writable %s\n", varname);
    return STAT_SET_UNKNOWN;
  }

  /* handle server side variable */
  if (hidups_item->hidflags & HU_FLAG_ABSENT) {
    upsdebugx(2, "setvar: setting server side variable %s\n", varname);
    dstate_setinfo(hidups_item->info_type, "%s", val);
    return STAT_SET_HANDLED;
  } else {
    /* SHUT_FLAG_ABSENT is the only case of HID Path == NULL */
    if (hidups_item->hidpath == NULL) {
      upsdebugx(2, "setvar: ID Path is NULL for %s\n", varname);
      return STAT_SET_UNKNOWN;
    }
  }

  /* Actual variable setting */
  if (HIDSetItemValue(hidups_item->hidpath, atol(val))) {

    /* TODO: GetValue(hidups_item->hidpath) to ensure success */
    upsdebugx(3, "SUCCEED\n");
    /* Delay a bit not to flood the device */
    sleep(1);
    /* Set the status so that SEMI_STATIC vars are polled */
    data_has_changed = TRUE;
    return STAT_SET_HANDLED;
  }
  else
    upsdebugx(3, "FAILED\n"); /* TODO: HANDLED but FAILED, not UNKNOWN! */

  return STAT_SET_UNKNOWN;
}

void upsdrv_help(void) { /* TODO: to be completed */ }

void upsdrv_makevartable(void) 
{
  char temp [128];
  
  /* add command line/conf variables */
  sprintf(temp, "Set shutdown delay, in seconds (default=%d).",
	  DEFAULT_OFFDELAY);
  addvar (VAR_VALUE, "offdelay", temp);
  
  sprintf(temp, "Set startup delay, in ten seconds units for MGE (default=%d).",
	  DEFAULT_ONDELAY);
  addvar (VAR_VALUE, "ondelay", temp);
  
  /* TODO: getval (Mfr, Prod, Idx) */
}

void upsdrv_banner(void)
{
  printf("Network UPS Tools: New HID UPS driver %s (%s)\n\n",
	 DRIVER_VERSION, UPS_VERSION);

  experimental_driver = 1;
}

/* TODO: merge {init,update}info in hidups_walk() */
void upsdrv_updateinfo(void) 
{
	hid_info_t *item;
	float value;
	char *nutvalue;
	int tries=0, retcode;

	upsdebugx(1, "\n=>Updating...");

	/* clear status buffer before begining */
	status_init();

	/* Process pending events (HID notifications on Interrupt pipe) */
	/* TODO: process "HIDItem HIDGetNextEvent(HIDDevice *dev);" */

	/* Device data walk ----------------------------- */
	for ( item = hid_ups ; item->info_type != NULL ; item++ )
	{
		/* These doesn't need polling after initinfo() */
		if ((item->hidflags & HU_FLAG_ABSENT)
			|| (item->hidflags & HU_TYPE_CMD)
			|| (item->hidflags & HU_FLAG_STATIC)
			/* These need to be polled after user changes (setvar / instcmd) */
			|| ( (item->hidflags & HU_FLAG_SEMI_STATIC) && (data_has_changed == FALSE) ) )
				continue;

		if ( (item->hidflags & HU_FLAG_SEMI_STATIC) && (data_has_changed == TRUE) )
			upsdebugx(2, "%s => HU_FLAG_SEMI_STATIC", item->hidpath);

		if (item->hidflags & HU_FLAG_OK)
		{
			tries = 0;
			value = -2;

			/* TODO: 3 tryes before giving up (needed when OnBattery!) */
			while ( tries < MAX_TRY )
			{
				tries++;
				retcode = HIDGetItemValue(item->hidpath, &value);

				/* Did we get something */
				if (retcode == 1 ) /* Yes */
				{
					tries = MAX_TRY;
					upsdebugx(2, "%s = %f", item->hidpath, value);

					/* deal with status items */
					if (!strncmp(item->info_type, "ups.status", 10)) {

						nutvalue = hu_find_infoval(item->hid2info, (long)value);
						if (nutvalue != NULL)	  
							status_set(nutvalue);

					} else { /* standard items */
						/* need lookup'ed translation */
						if (item->hid2info != NULL) {
							nutvalue = hu_find_infoval(item->hid2info, (long)value);
							if (nutvalue != NULL)
								dstate_setinfo(item->info_type, item->dfl, nutvalue);
						}
						else
							dstate_setinfo(item->info_type, item->dfl, value);

						dstate_dataok(); /* atomic "commit" call */
					}
				} else {
					upsdebugx(2, "Can't get %s (retcode = %i)", item->hidpath, retcode);
					if ( tries == (MAX_TRY -1) )
						dstate_datastale();
					/* Reserved values: -1/-10 for nul delay, -2 can't get value */
					/* device has been disconnected, try to reconnect */
					/*if ( (retcode != -1) && (retcode != -EPIPE) )
					{
						HIDCloseDevice(NULL);
						sleep(3);
						hd = HIDOpenDevice(device_path, &flg, MODE_REOPEN);
					} */
				}
			} 
		}
	}
	/* Commit the status buffer */
	status_commit();

	/* Reset SEMI_STATIC flag */
	data_has_changed = FALSE;
}

void upsdrv_initinfo(void)
{
	hid_info_t *item;
	float value;
	char *nutvalue;
	int retcode;

	/* identify unit: fill ups.{mfr, model, serial} */
	dstate_setinfo("ups.mfr", "%s", hd->Vendor);
	dstate_setinfo("ups.model", "%s", format_model_name(hd->Product, NULL));
	dstate_setinfo("ups.serial", "%s", 
		(hd->Serial != NULL)?hd->Serial:"unknown");

	/* TODO: load lookup file (WARNING: should be in initups()
	because of -k segfault (=> not calling upsdrv_initinfo())
	*/

	/* clear status buffer before begining */
	status_init();

	/* Device capabilities enumeration ----------------------------- */
	for ( item = hid_ups ; item->info_type != NULL ; item++ )
	{
		/* Avoid redundancy when multiple defines (RO/RW)
		* Not applicable to "ups.status" items!
		*/

		if ((dstate_getinfo(item->info_type) != NULL)
			&& (strncmp(item->info_type, "ups.status", 10)))
		{
			item->hidflags &= ~HU_FLAG_OK;
			continue;
		}

		/* Special case for handling instant commands */
		if (item->hidflags & HU_TYPE_CMD)
		{
			/* Check actual availability */
			if (HIDGetItemValue(item->hidpath, &value) == 1 )
			{
				dstate_addcmd(item->info_type);
			}
			continue;
		}

		/* Special case for handling server side variables */
		if (item->hidflags & HU_FLAG_ABSENT)
		{
			dstate_setinfo(item->info_type, "%s", item->dfl);
			dstate_setflags(item->info_type, item->info_flags);

			/* disable reading now => needed?!
			item->shut_flags &= ~HU_FLAG_OK;*/
		}
		else
		{
			/* TODO: 3 tryes before giving up (needed when OnBattery!) */
			if ((retcode = HIDGetItemValue(item->hidpath, &value)) == 1 )
			{
				/* not needed as it's already set => item->hidflags &= HU_FLAG_OK; */

				/* deal with status items */
				if (!strncmp(item->info_type, "ups.status", 10))
				{
					nutvalue = hu_find_infoval(item->hid2info, (long)value);
					if (nutvalue != NULL)
						status_set(nutvalue);
				}
				else /* standard items */
				{
					/* need lookup'ed translation */
					if (item->hid2info != NULL)
					{
						nutvalue = hu_find_infoval(item->hid2info, (long)value);
						if (nutvalue != NULL)
							dstate_setinfo(item->info_type, item->dfl, nutvalue);
					}
					else
						dstate_setinfo(item->info_type, item->dfl, value);

					/* TODO: verify setability/RW with (hData.Attribute != ATTR_DATA_CST) */
					dstate_setflags(item->info_type, item->info_flags);
				}

				/* Set max length for strings */
				if (item->info_flags & ST_FLAG_STRING)
					dstate_setaux(item->info_type, item->info_len);

				dstate_dataok(); /* atomic call */
			}
			else
			{
				/* invalidate item */
				item->hidflags &= ~HU_FLAG_OK;
			}
		}
	}

	/* Commit the status buffer */
	status_commit();

	/* install handlers */
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;
}

void upsdrv_initups(void)
{
  /* Search for the first UPS, no matter Mfr or exact product */
  /* TODO: add vartable for VID, PID, Idx => getval*/
  flg.VendorID = MGE_UPS_SYSTEMS; /* limiting to MGE is enough for now! */
  flg.ProductID = ANY;
  flg.UsageCode = hid_lookup_usage("UPS");
  flg.Index = 0x0;
  
  if ((hd = HIDOpenDevice(device_path, &flg, MODE_OPEN)) == NULL)
    fatalx("No USB/HID UPS found");
  else
    upslogx(1, "Detected an UPS: %s/%s\n", hd->Vendor, hd->Product);

  /* See initinfo for WARNING */
  switch (hd->VendorID)
    {
    case MGE_UPS_SYSTEMS:
		hid_ups = hid_mge;
		break;
    case APC:
    case MUSTEK:
    default:
      upslogx(1, "Manufacturer not supported.\nContact driver author <arnaud.quette@free.fr> or <arnaud.quette@mgeups.com> with the below information");
      HIDDumpTree(NULL);
      fatalx("Aborting");
      break;       
    }
}

void upsdrv_cleanup(void)
{
  if (hd != NULL)
    HIDCloseDevice(hd);
}

/**********************************************************************
 * Support functions
 *********************************************************************/

/* find info element definition in my info array. */
static hid_info_t *find_info(const char *varname)
{
  hid_info_t *hidups_item;

  for (hidups_item = hid_ups; hidups_item->info_type != NULL ; hidups_item++) {
    if (!strcasecmp(hidups_item->info_type, varname))
      return hidups_item;
  }

  upsdebugx(2, "find_info: unknown info type: %s\n", varname);
  return NULL;
}

/* find the HID Item value matching that NUT value */
/* static long hu_find_valinfo(info_lkp_t *hid2info, char* value)
{
  info_lkp_t *info_lkp;
  
  for (info_lkp = hid2info; (info_lkp != NULL) &&
	 (strcmp(info_lkp->nut_value, "NULL")); info_lkp++) {
    
    if (!(strcmp(info_lkp->nut_value, value))) {
      upsdebugx(3, "hu_find_valinfo: found %s (value: %s)\n",
		info_lkp->nut_value, value);
      
      return info_lkp->hid_value;
    }
  }
  upsdebugx(3, "hu_find_valinfo: no matching HID value for this INFO_* value (%s)", value);
  return -1;
} */

/* find the NUT value matching that HID Item value */
static char *hu_find_infoval(info_lkp_t *hid2info, long value)
{
  info_lkp_t *info_lkp;
  
  upsdebugx(3, "hu_find_infoval: searching for value = %ld\n", value);
  
  for (info_lkp = hid2info; (info_lkp != NULL) &&
	 (strcmp(info_lkp->nut_value, "NULL")); info_lkp++) {
    
    if (info_lkp->hid_value == value) {
      upsdebugx(3, "hu_find_infoval: found %s (value: %ld)\n",
		info_lkp->nut_value, value);
      
      return info_lkp->nut_value;
    }
  }
  upsdebugx(3, "hu_find_infoval: no matching INFO_* value for this HID value (%ld)\n", value);
  return NULL;
}

/* All the logic for formatting finely the UPS model name */
static char *format_model_name(char *iProduct, char *iModel)
{
	models_name_t *model = NULL;
	char *new_name = NULL;
	float value;
	
  upsdebugx(2, "Searching for %s\n", iProduct);

  /* Search for formatting rules */
  for ( model = models_names ; model->VendorID != 0x0 ; model++ ) {
    if(model->VendorID == hd->VendorID) {
      if ((iProduct != NULL) && (!strncmp(iProduct, model->basename, model->size))) {
	upsdebugx(2, "Found %s\n", model->finalname);
	break;
      }
      else
	continue;
    }
    else
      continue;
  }

  /* Process data */
  switch (model->VendorID)
    {
    case MGE_UPS_SYSTEMS:
      if (!strncmp(iProduct, "ELLIPSE", 7)) {      
	if (HIDGetItemValue(model->data2, &value) == 1)
	  sprintf(iProduct, "%s %i", model->finalname, (int)value);
	else
	  sprintf(iProduct, model->finalname);
      } else {
	if (HIDGetItemString(model->data2) != NULL)
	  sprintf(iProduct, "%s %s", model->finalname, 
		  HIDGetItemString(model->data2)+model->d2_offset);
	else
	  sprintf(iProduct, model->finalname);
      }
      new_name = iProduct;
      break; 
    case APC:
      /* TODO: finish this code, use data2 as an strstr() */
    default:
      new_name = "Generic USB UPS";
    }
  return new_name;
}
