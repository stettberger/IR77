/* vim:fdm=marker ts=4 et ai
 * {{{
 *
 * (c) by Alexander Neumann <alexander@bumpern.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * For more information on the GPL, please go to:
 * http://www.gnu.org/copyleft/gpl.html
 }}} */

#include "eeprom.h"

#ifdef DEBUG
#include "uart.h"
#endif

void eeprom_load_ip(uint8_t *addr, uip_ipaddr_t *dest)
/* {{{ */ {

    uint8_t ip[4];

    for (uint8_t i = 0; i < 4; i++)
        ip[i] = eeprom_read_byte(&addr[i]);

    uip_ipaddr(*dest, ip[0], ip[1], ip[2], ip[3]);

} /* }}} */