/**
 * @file
 * @brief	Explicit booleans.
 * @copyright	@parblock
 * Copyright (C):
 * - 2018 -- Daniele Pezzini (<hyouko@gmail.com>)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *		@endparblock
 */

#ifndef BOOL_H
#define BOOL_H

/** @brief Boolean type: either @ref TRUE, or @ref FALSE. */
#ifndef TRUE
typedef enum {
	FALSE,	/**< Anything that is not true... must be false. */
	TRUE	/**< Anything that is not false... must be true. */
}		bool_t;
#else	/* TRUE */
typedef int	bool_t;
#endif	/* TRUE */

#endif	/* BOOL_H */
