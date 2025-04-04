/* nutdrv_qx_blazer-common.h - Common functions/settings for nutdrv_qx_{innovart31,mecer,megatec,megatec-old,mustek,q1,q2,q6,voltronic-qs,zinto}.{c,h}
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
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
 *
 */

#ifndef NUTDRV_QX_BLAZER_COMMON_H
#define NUTDRV_QX_BLAZER_COMMON_H

#include "nutdrv_qx.h"

/* Support functions */
void	blazer_makevartable(void);
void	blazer_makevartable_light(void);
void	blazer_initups(item_t *qx2nut);
void	blazer_initups_light(item_t *qx2nut);
int	blazer_claim(void);
int	blazer_claim_light(void);

/* Preprocess functions */
int	blazer_process_command(item_t *item, char *value, const size_t valuelen);
int	blazer_process_setvar(item_t *item, char *value, const size_t valuelen);
int	blazer_process_status_bits(item_t *item, char *value, const size_t valuelen);

/* Ranges */
extern info_rw_t	blazer_r_ondelay[];
extern info_rw_t	blazer_r_offdelay[];

#endif /* NUTDRV_QX_BLAZER_COMMON_H */
