/* vim:fdm=marker ts=4 et ai
 * {{{
 *
 * Copyright (c) by Alexander Neumann <alexander@bumpern.de>
 * Copyright (c) 2007 by Stefan Siegl <stesie@brokenpipe.de>
 * Copyright (c) 2007 by Christian Dietrich <stettberger@dokucode.de>
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

#include <string.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

#include "../config.h"
#include "../debug.h"
#include "../uip/uip.h"
#include "../uip/uip_arp.h"
#include "../eeprom.h"
#include "../bit-macros.h"
#include "../fs20/fs20.h"
#include "../portio.h"
#include "../lcd/hd44780.h"
#include "../named_pin/named_pin.h"
#include "../onewire/onewire.h"
#include "../rc5/rc5.h"
#include "ecmd.h"


/* module local prototypes */
/* high level */
static int16_t parse_cmd_ip(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_mac(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_show_ip(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_show_version(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_show_mac(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_bootloader(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_reset(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_io_set_ddr(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_io_get_ddr(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_io_set_port(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_io_get_port(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_io_get_pin(char *cmd, char *output, uint16_t len);
#ifdef NAMED_PIN_SUPPORT
static int16_t parse_cmd_pin_get(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_pin_set(char *cmd, char *output, uint16_t len);
static int16_t parse_cmd_pin_toggle(char *cmd, char *output, uint16_t len);
#endif
#ifdef FS20_SUPPORT
#ifdef FS20_SUPPORT_SEND
static int16_t parse_cmd_send_fs20(char *cmd, char *output, uint16_t len);
#endif
#ifdef FS20_SUPPORT_RECEIVE
static int16_t parse_cmd_recv_fs20(char *cmd, char *output, uint16_t len);
#ifdef FS20_SUPPORT_RECEIVE_WS300
static int16_t parse_cmd_recv_fs20_ws300(char *cmd, char *output, uint16_t len);
#endif
#endif
#endif /* FS20_SUPPORT */
#ifdef HD44780_SUPPORT
static int16_t parse_lcd_clear(char *cmd, char *output, uint16_t len);
static int16_t parse_lcd_write(char *cmd, char *output, uint16_t len);
static int16_t parse_lcd_goto(char *cmd, char *output, uint16_t len);
#endif
#ifdef ONEWIRE_SUPPORT
static int16_t parse_onewire_list(char *cmd, char *output, uint16_t len);
static int16_t parse_onewire_get(char *cmd, char *output, uint16_t len);
static int16_t parse_onewire_convert(char *cmd, char *output, uint16_t len);
#endif
#ifdef RC5_SUPPORT
static int16_t parse_ir_send(char *cmd, char *output, uint16_t len);
static int16_t parse_ir_receive(char *cmd, char *output, uint16_t len);
#endif


/* low level */
static int8_t parse_ip(char *cmd, uint8_t *ptr);
static int8_t parse_mac(char *cmd, uint8_t *ptr);
static int8_t parse_ow_rom(char *cmd, uint8_t *ptr);

/* struct for storing commands */
struct ecmd_command_t {
    PGM_P name;
    int16_t (*func)(char*, char*, uint16_t);
};

/* construct strings.  this is ugly, but the only known way of
 * storing structs containing string pointer completely in program
 * space */
const char PROGMEM ecmd_showmac_text[] = "show mac";
const char PROGMEM ecmd_showip_text[] = "show ip";
const char PROGMEM ecmd_showversion_text[] = "show version";
const char PROGMEM ecmd_ip_text[] = "ip ";
const char PROGMEM ecmd_mac_text[] = "mac ";
const char PROGMEM ecmd_bootloader_text[] = "bootloader";
const char PROGMEM ecmd_reset_text[] = "reset";
const char PROGMEM ecmd_io_set_ddr[] = "io set ddr";
const char PROGMEM ecmd_io_get_ddr[] = "io get ddr";
const char PROGMEM ecmd_io_set_port[] = "io set port";
const char PROGMEM ecmd_io_get_port[] = "io get port";
const char PROGMEM ecmd_io_get_pin[] = "io get pin";
#ifdef NAMED_PIN_SUPPORT
const char PROGMEM ecmd_pin_get[] = "pin get";
const char PROGMEM ecmd_pin_set[] = "pin set";
const char PROGMEM ecmd_pin_toggle[] = "pin toggle";
#endif
#ifdef FS20_SUPPORT
#ifdef FS20_SUPPORT_SEND
const char PROGMEM ecmd_fs20_send_text[] = "fs20 send";
#endif
#ifdef FS20_SUPPORT_RECEIVE
const char PROGMEM ecmd_fs20_recv_text[] = "fs20 receive";
#ifdef FS20_SUPPORT_RECEIVE_WS300
const char PROGMEM ecmd_fs20_recv_ws300_text[] = "fs20 ws300";
#endif
#endif
#endif /* FS20_SUPPORT */
#ifdef HD44780_SUPPORT
const char PROGMEM ecmd_lcd_clear_text[] = "lcd clear";
const char PROGMEM ecmd_lcd_write_text[] = "lcd write";
const char PROGMEM ecmd_lcd_goto_text[] = "lcd goto";
#endif
#ifdef ONEWIRE_SUPPORT
const char PROGMEM ecmd_onewire_list[] = "1w list";
const char PROGMEM ecmd_onewire_get[] = "1w get";
const char PROGMEM ecmd_onewire_convert[] = "1w convert";
#endif
#ifdef RC5_SUPPORT
const char PROGMEM ecmd_ir_send[] = "ir send";
const char PROGMEM ecmd_ir_receive[] = "ir receive";
#endif

const struct ecmd_command_t PROGMEM ecmd_cmds[] = {
    { ecmd_ip_text, parse_cmd_ip },
    { ecmd_showmac_text, parse_cmd_show_mac },
    { ecmd_showip_text, parse_cmd_show_ip },
    { ecmd_showversion_text, parse_cmd_show_version },
    { ecmd_mac_text, parse_cmd_mac },
    { ecmd_bootloader_text, parse_cmd_bootloader }, 
    { ecmd_reset_text, parse_cmd_reset },
    { ecmd_io_set_ddr, parse_cmd_io_set_ddr },
    { ecmd_io_get_ddr, parse_cmd_io_get_ddr },
    { ecmd_io_set_port, parse_cmd_io_set_port },
    { ecmd_io_get_port, parse_cmd_io_get_port },
    { ecmd_io_get_pin, parse_cmd_io_get_pin },
#ifdef NAMED_PIN_SUPPORT
    { ecmd_pin_get, parse_cmd_pin_get },
    { ecmd_pin_set, parse_cmd_pin_set },
    { ecmd_pin_toggle, parse_cmd_pin_toggle },
#endif
#ifdef FS20_SUPPORT 
#ifdef FS20_SUPPORT_SEND
    { ecmd_fs20_send_text, parse_cmd_send_fs20 },
#endif
#ifdef FS20_SUPPORT_RECEIVE
    { ecmd_fs20_recv_text, parse_cmd_recv_fs20 },
#ifdef FS20_SUPPORT_RECEIVE_WS300
    { ecmd_fs20_recv_ws300_text, parse_cmd_recv_fs20_ws300 },
#endif
#endif
#endif /* FS20_SUPPORT */
#ifdef HD44780_SUPPORT
    { ecmd_lcd_clear_text, parse_lcd_clear },
    { ecmd_lcd_write_text, parse_lcd_write },
    { ecmd_lcd_goto_text, parse_lcd_goto },
#endif
#ifdef ONEWIRE_SUPPORT
    { ecmd_onewire_list, parse_onewire_list },
    { ecmd_onewire_get, parse_onewire_get },
    { ecmd_onewire_convert, parse_onewire_convert },
#endif
#ifdef RC5_SUPPORT
    { ecmd_ir_send, parse_ir_send },
    { ecmd_ir_receive, parse_ir_receive },
#endif
    { NULL, NULL },
};

int16_t ecmd_parse_command(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD
    debug_printf("called ecmd_parse_command\n");
#endif

    if (strlen(cmd) <= 2) {
#ifdef DEBUG_ECMD
        debug_printf("cmd is too short\n");
#endif
        return 0;
    }

    int ret = -1;

    char *text = NULL;
    int16_t (*func)(char*, char*, uint16_t) = NULL;
    uint8_t pos = 0;

    while (1) {
        /* load pointer to text */
        text = (char *)pgm_read_word(&ecmd_cmds[pos].name);

#ifdef DEBUG_ECMD
        debug_printf("loaded text addres %p: \n", text);
#endif

        /* return if we reached the end of the array */
        if (text == NULL)
            break;

#ifdef DEBUG_ECMD
        debug_printf("text is: \"");
        printf_P(text);
        debug_printf("\"\n");
#endif

        /* else compare texts */
        if (strncasecmp_P(cmd, text, strlen_P(text)) == 0) {
#ifdef DEBUG_ECMD
            debug_printf("found match\n");
#endif
            cmd += strlen_P(text);
            func = (void *)pgm_read_word(&ecmd_cmds[pos].func);
            break;
        }

        pos++;
    }

#ifdef DEBUG_ECMD
    debug_printf("rest cmd: \"%s\"\n", cmd);
#endif

    if (func != NULL)
        ret = func(cmd, output, len);

    if (ret == -1 && output != NULL) {
        strncpy_P(output, PSTR("parse error"), len);
        ret = 12;
    } else if (ret == 0) {
        strncpy_P(output, PSTR("OK"), len);
        ret = 2;
    }

    return ret;
} /* }}} */

/* high level parsing functions */

int16_t parse_cmd_bootloader(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    cfg.request_bootloader = 1;
    uip_close();
    return 0;
} /* }}} */

int16_t parse_cmd_show_mac(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD_MAC
    debug_printf("called parse_cmd_show with rest: \"%s\"\n", cmd);
#endif

    struct uip_eth_addr buf;
    uint8_t *mac = (uint8_t *)&buf;

    eeprom_read_block(&buf, EEPROM_MAC_OFFSET, sizeof(struct uip_eth_addr));

    int output_len = snprintf_P(output, len,
            PSTR("mac %02x:%02x:%02x:%02x:%02x:%02x"),
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return output_len;
} /* }}} */

int16_t parse_cmd_show_ip(char *cmd, char *output, uint16_t len)
/* {{{ */ {
#if UIP_CONF_IPV6
    return -1;
#else 
    uint8_t ips[sizeof(uip_ipaddr_t)*3];

    eeprom_read_block(ips, EEPROM_IPS_OFFSET, sizeof(uip_ipaddr_t)*3);

    int output_len = snprintf_P(output, len,
            PSTR("ip %u.%u.%u.%u/%u.%u.%u.%u, gateway %u.%u.%u.%u"),
            ips[0], ips[1], ips[2], ips[3],
            ips[4], ips[5], ips[6], ips[7],
            ips[8], ips[9], ips[10], ips[11]);

    return output_len;
#endif /* ! UIP_CONF_IPV6 */
} /* }}} */

int16_t parse_cmd_show_version(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    return snprintf_P(output, len,
            PSTR("version %s"), VERSION_STRING);
} /* }}} */

static int16_t parse_cmd_ip(char *cmd, char *output, uint16_t len)
/* {{{ */ {
#if UIP_CONF_IPV6
    return -1;
#else 

#ifdef DEBUG_ECMD_IP
    debug_printf("called with string %s\n", cmd);
#endif

    int8_t ret;

    uip_ipaddr_t ips[3];

    for (uint8_t i = 0; i < 3; i++) {

        if (cmd == NULL)
            return -1;

        while (*cmd == ' ')
            cmd++;

        /* try to parse ip */
        ret = parse_ip(cmd, (uint8_t *)&ips[i]);

        /* locate next whitespace char */
        cmd = strchr(cmd, ' ');

#ifdef DEBUG_ECMD_IP
        debug_printf("next string is '%s', ret is %d\n", cmd, ret);
#endif

        if (ret < 0)
            return -1;

#ifdef DEBUG_ECMD_IP
        debug_printf("successfully parsed ip param %u\n", i);
#endif
    }

#ifdef DEBUG_ECMD
    debug_printf("saving new network configuration\n");
#endif

    /* save new ip addresses, use uip_buf since this buffer is unused when
     * this function is executed */
    return eeprom_save_config(NULL, ips[0], ips[1], ips[2]);
#endif /* ! UIP_CONF_IPV6 */

} /* }}} */

static int16_t parse_cmd_mac(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD_MAC
    debug_printf("called with string %s\n", cmd);
#endif

    int8_t ret;

    /* allocate space for mac */
    struct uip_eth_addr mac;

    ret = parse_mac(cmd, (void *)&mac);

    if (ret >= 0)
        return eeprom_save_config(&mac, NULL, NULL, NULL);
    else
        return ret;

} /* }}} */

static int16_t parse_cmd_reset(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    cfg.request_reset = 1;
    uip_close();
    return 0;
} /* }}} */

#ifdef FS20_SUPPORT 
#ifdef FS20_SUPPORT_SEND
static int16_t parse_cmd_send_fs20(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD_FS20
    debug_printf("called with string %s\n", cmd);
#endif

    uint16_t hc, addr, c;

    int ret = sscanf_P(cmd,
            PSTR("%x %x %x"),
            &hc, &addr, &c);

    if (ret == 3) {
#ifdef DEBUG_ECMD_FS20
        debug_printf("fs20_send(0x%x,0x%x,0x%x)\n", hc, LO8(addr), LO8(c));
#endif

        fs20_send(hc, LO8(addr), LO8(c));
        return 0;
    }

    return -1;

} /* }}} */
#endif

#ifdef FS20_SUPPORT_RECEIVE
static int16_t parse_cmd_recv_fs20(char *cmd, char *output, uint16_t len)
/* {{{ */ {

    char *s = output;
    uint8_t l = 0;
    uint8_t outlen = 0;

#ifdef DEBUG_ECMD_FS20
    debug_printf("%u positions in queue\n", fs20_global.fs20.len);
#endif

    while (l < fs20_global.fs20.len &&
            (uint8_t)(outlen+9) < len) {
#ifdef DEBUG_ECMD_FS20
        debug_printf("generating for pos %u: %02x%02x%02x%02x", l,
                fs20_global.fs20.queue[l].hc1,
                fs20_global.fs20.queue[l].hc2,
                fs20_global.fs20.queue[l].addr,
                fs20_global.fs20.queue[l].cmd);
#endif

        sprintf_P(s, PSTR("%02x%02x%02x%02x\n"),
                fs20_global.fs20.queue[l].hc1,
                fs20_global.fs20.queue[l].hc2,
                fs20_global.fs20.queue[l].addr,
                fs20_global.fs20.queue[l].cmd);

        s += 9;
        outlen += 9;
        l++;

#ifdef DEBUG_ECMD_FS20
        *s = '\0';
        debug_printf("output is \"%s\"\n", output);
#endif
    }

    /* clear queue */
    fs20_global.fs20.len = 0;

    return outlen;

} /* }}} */

#ifdef FS20_SUPPORT_RECEIVE_WS300
static int16_t parse_cmd_recv_fs20_ws300(char *cmd, char *output, uint16_t len)
/* {{{ */ {

    return snprintf_P(output, len,
            PSTR("deg: %u.%u C, hyg: %u%%, wind: %u.%u km/h, rain: %u, counter: %u"),
            fs20_global.ws300.temp,
            fs20_global.ws300.temp_frac,
            fs20_global.ws300.hygro,
            fs20_global.ws300.wind,
            fs20_global.ws300.wind_frac,
            fs20_global.ws300.rain,
            fs20_global.ws300.rain_value);

} /* }}} */
#endif
#endif
#endif /* FS20_SUPPORT */

#ifdef ONEWIRE_SUPPORT
static int16_t parse_onewire_list(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    int16_t ret;

    if (ow_global.lock == 0) {
        ow_global.lock = 1;
#ifdef DEBUG_ECMD_OW_LIST
        debug_printf("called onewire list for the first time\n");
#endif

        /* disable interrupts */
        uint8_t sreg = SREG;
        cli();

        ret = ow_search_rom_first();

        /* re-enable interrupts */
        SREG = sreg;

        if (ret <= 0) {
#ifdef DEBUG_ECMD_OW_LIST
            debug_printf("no devices on the bus\n");
#endif
            return 0;
        }
    } else {
#ifdef DEBUG_ECMD_OW_LIST
        debug_printf("called onewire list again\n");
#endif

        /* disable interrupts */
        uint8_t sreg = SREG;
        cli();

        ret = ow_search_rom_next();

        SREG = sreg;
    }

    if (ret == 1) {
#ifdef DEBUG_ECMD_OW_LIST
        debug_printf("discovered a device: "
                "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                ow_global.current_rom.bytewise[0],
                ow_global.current_rom.bytewise[1],
                ow_global.current_rom.bytewise[2],
                ow_global.current_rom.bytewise[3],
                ow_global.current_rom.bytewise[4],
                ow_global.current_rom.bytewise[5],
                ow_global.current_rom.bytewise[6],
                ow_global.current_rom.bytewise[7]);
#endif
        ret = snprintf_P(output, len,
                PSTR("%02x%02x%02x%02x%02x%02x%02x%02x"),
                ow_global.current_rom.bytewise[0],
                ow_global.current_rom.bytewise[1],
                ow_global.current_rom.bytewise[2],
                ow_global.current_rom.bytewise[3],
                ow_global.current_rom.bytewise[4],
                ow_global.current_rom.bytewise[5],
                ow_global.current_rom.bytewise[6],
                ow_global.current_rom.bytewise[7]);

#ifdef DEBUG_ECMD_OW_LIST
        debug_printf("generated %d bytes\n", ret);
#endif

        /* set return value that the parser has to be called again */
        if (ret > 0)
            ret = -ret - 10;

#ifdef DEBUG_ECMD_OW_LIST
        debug_printf("returning %d\n", ret);
#endif
        return ret;

    } else if (ret == 0) {
        ow_global.lock = 0;
        return 0;
    }

    return -1;
} /* }}} */

static int16_t parse_onewire_get(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    int16_t ret;

    cmd++;
    debug_printf("called onewire_list with: \"%s\"\n", cmd);

    struct ow_rom_code_t rom;

    ret = parse_ow_rom(cmd, (void *)&rom);

    /* check for parse error */
    if (ret < 0)
        return -1;

    if (ow_temp_sensor(&rom)) {
        debug_printf("reading temperature\n");

        /* disable interrupts */
        uint8_t sreg = SREG;
        cli();

        struct ow_temp_scratchpad_t sp;
        ret = ow_temp_read_scratchpad(&rom, &sp);

        /* re-enable interrupts */
        SREG = sreg;

        if (ret != 1) {
            debug_printf("scratchpad read failed: %d\n", ret);
            return -2;
        }

        debug_printf("successfully read scratchpad\n");

        uint16_t temp = ow_temp_normalize(&rom, &sp);

        debug_printf("temperature: %d.%d\n", HI8(temp), LO8(temp) > 0 ? 5 : 0);

        ret = snprintf_P(output, len,
                PSTR("temperature: %d.%d\n"),
                HI8(temp), LO8(temp) > 0 ? 5 : 0);
    } else if (ow_eeprom(&rom)) {
        debug_printf("reading mac\n");

        /* disable interrupts */
        uint8_t sreg = SREG;
        cli();

        uint8_t mac[6];
        ret = ow_eeprom_read(&rom, mac);

        /* re-enable interrupts */
        SREG = sreg;

        if (ret != 0) {
            debug_printf("mac read failed: %d\n", ret);
            return -2;
        }

        debug_printf("successfully read mac\n");

        debug_printf("mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        ret = snprintf_P(output, len,
                PSTR("mac: %02x:%02x:%02x:%02x:%02x:%02x"),
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    } else {
        debug_printf("unknown sensor type\n");
        ret = snprintf_P(output, len, PSTR("unknown sensor type"));
    }

    return ret;
} /* }}} */

static int16_t parse_onewire_convert(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    int16_t ret;

    if (strlen(cmd) > 0)
        cmd++;

    debug_printf("called onewire_list with: \"%s\"\n", cmd);

    struct ow_rom_code_t rom, *romptr;

    ret = parse_ow_rom(cmd, (void *)&rom);

    /* check for romcode */
    if (ret < 0)
        romptr = NULL;
    else
        romptr = &rom;

    debug_printf("converting temperature...\n");

    /* disable interrupts */
    uint8_t sreg = SREG;
    cli();

    ret = ow_temp_start_convert_wait(romptr);

    SREG = sreg;

    if (ret == 1)
        /* done */
        return 0;
    else if (ret == -1)
        /* no device attached */
        return -2;
    else
        /* wrong rom family code */
        return -1;

} /* }}} */
#endif

#ifdef RC5_SUPPORT
static int16_t parse_ir_send(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    int16_t ret;

    uint16_t addr, command;

    ret = sscanf_P(cmd, PSTR("%d %d"), &addr, &command);

    debug_printf("sending ir: device %d, command %d\n", addr, command);

    /* check if two values have been given */
    if (ret != 2)
        return -1;

    rc5_send(LO8(addr), LO8(command));
    return 0;

} /* }}} */

static int16_t parse_ir_receive(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    char *s = output;
    uint8_t l = 0;
    uint8_t outlen = 0;

#ifdef DEBUG_ECMD_RC5
    debug_printf("%u positions in queue\n", rc5_global.len);
#endif

    while (l < rc5_global.len && (uint8_t)(outlen+5) < len) {
#ifdef DEBUG_ECMD_RC5
        debug_printf("generating for pos %u: %02u/%02u", l,
                rc5_global.queue[l].address,
                rc5_global.queue[l].code);
#endif

        sprintf_P(s, PSTR("%02u%02u\n"),
                rc5_global.queue[l].address,
                rc5_global.queue[l].code);

        s += 5;
        outlen += 5;
        l++;

#ifdef DEBUG_ECMD_RC5
        *s = '\0';
        debug_printf("output is \"%s\"\n", output);
#endif
    }

    /* clear queue */
    rc5_global.len = 0;

    return outlen;
} /* }}} */
#endif

static int16_t parse_cmd_io_set_ddr(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD_PORTIO
    debug_printf("called parse_cmd_io_set_ddr with rest: \"%s\"\n", cmd);
#endif

    uint16_t port, data, mask;

    int ret = sscanf_P(cmd,
            PSTR("%x %x %x"),
            &port, &data, &mask);

    /* use default mask, if no mask has been given */
    if (ret == 2) {
        mask = 0xff;
        ret = 3;
    }

    if (ret == 3 && port < IO_PORTS) {

        cfg.options.io_ddr[port] = (cfg.options.io_ddr[port] & ~mask) |
                                   LO8(data & mask);

        return 0;
    } else
        return -1;

} /* }}} */

static int16_t parse_cmd_io_get_ddr(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD_PORTIO
    debug_printf("called parse_cmd_io_get_ddr with rest: \"%s\"\n", cmd);
#endif

    uint16_t port;

    int ret = sscanf_P(cmd,
            PSTR("%x"),
            &port);

    if (ret == 1 && port < IO_PORTS) {

        return snprintf_P(output, len,
                PSTR("port %d: 0x%02x"),
                port,
                cfg.options.io_ddr[port]);
    } else
        return -1;

} /* }}} */

static int16_t parse_cmd_io_set_port(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD_PORTIO
    debug_printf("called parse_cmd_io_set_port with rest: \"%s\"\n", cmd);
#endif

    uint16_t port, data, mask;

    int ret = sscanf_P(cmd,
            PSTR("%x %x %x"),
            &port, &data, &mask);

    /* use default mask, if no mask has been given */
    if (ret == 2) {
        mask = 0xff;
        ret = 3;
    }

    if (ret == 3 && port < IO_PORTS) {

        cfg.options.io[port] = (cfg.options.io[port] & ~mask) |
                                   LO8(data & mask);

        return 0;
    } else
        return -1;

} /* }}} */

static int16_t parse_cmd_io_get_port(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD_PORTIO
    debug_printf("called parse_cmd_io_get_port with rest: \"%s\"\n", cmd);
#endif

    uint16_t port;

    int ret = sscanf_P(cmd,
            PSTR("%x"),
            &port);

    if (ret == 1 && port < IO_PORTS) {

        return snprintf_P(output, len,
                PSTR("port %d: 0x%02x"),
                port,
                cfg.options.io[port]);
    } else
        return -1;

} /* }}} */

static int16_t parse_cmd_io_get_pin(char *cmd, char *output, uint16_t len)
/* {{{ */ {

#ifdef DEBUG_ECMD_PORTIO
    debug_printf("called parse_cmd_io_get_pin with rest: \"%s\"\n", cmd);
#endif

    uint16_t port;

    int ret = sscanf_P(cmd,
            PSTR("%x"),
            &port);

    if (ret == 1 && port < IO_PORTS) {

        return snprintf_P(output, len,
                PSTR("port %d: 0x%02x"),
                port,
                portio_input(port));
    } else
        return -1;

} /* }}} */

#ifdef NAMED_PIN_SUPPORT
static int16_t parse_cmd_pin_get(char *cmd, char *output, uint16_t len)
/* {{{ */ {
  uint16_t port, pin;

  uint8_t ret = sscanf_P(cmd, PSTR("%u %u"), &port, &pin);
  /* Fallback to named pins */
  if ( ret != 2 && *cmd) {
    uint8_t pincfg = named_pin_by_name(cmd + 1);
    if (pincfg != 255) {
        port = pgm_read_byte(&portio_pincfg[pincfg].port);
        pin = pgm_read_byte(&portio_pincfg[pincfg].pin);
        ret = 2;
    }
  }
  if (ret == 2 && port < IO_PORTS && pin < 8) {
    uint8_t pincfg = named_pin_by_pin(port, pin);
    uint8_t active_high = 1;
    if (pincfg != 255)  
      active_high = pgm_read_byte(&portio_pincfg[pincfg].active_high);
    return snprintf_P(output, len, 
                      XOR_LOG(((portio_input(port)) & _BV(pin)), !(active_high))
                      ? PSTR("on") : PSTR("off"));
  } else
    return -1;
}
/* }}} */

static int16_t parse_cmd_pin_set(char *cmd, char *output, uint16_t len)
/* {{{ */ {
  uint16_t port, pin, on;

  /* Parse String */
  uint8_t ret = sscanf_P(cmd, PSTR("%u %u %u"), &port, &pin, &on);
  /* Fallback to named pins */
  if ( ret != 3 && *cmd) {
    char *ptr = strchr(cmd + 1, ' ');
    if (ptr) {
      *ptr = 0;
      uint8_t pincfg = named_pin_by_name(cmd + 1);
      if (pincfg != 255) {
        port = pgm_read_byte(&portio_pincfg[pincfg].port);
        pin = pgm_read_byte(&portio_pincfg[pincfg].pin);
        if (ptr[1]) {
          ptr++;
          if(sscanf_P(ptr, PSTR("%u"), &on) == 1)
            ret = 3;
          else {
            if (strcmp_P(ptr, PSTR("on")) == 0) {
              on = 1;
              ret = 3;
            }
            else if (strcmp_P(ptr, PSTR("off")) == 0) {
              on = 0;
              ret = 3;
            }
          }
        }
      }
    }
  }

  if (ret == 3 && port < IO_PORTS && pin < 8) {
    /* Set only if it is output */
    if (cfg.options.io_ddr[port] & _BV(pin)) {
      uint8_t pincfg = named_pin_by_pin(port, pin);
      uint8_t active_high = 1;
      if (pincfg != 255)  
        active_high = pgm_read_byte(&portio_pincfg[pincfg].active_high);

      if (XOR_LOG(on, !active_high)) 
        cfg.options.io[port] = (cfg.options.io[port] ) | _BV(pin);
      else
        cfg.options.io[port] = (cfg.options.io[port] ) & ~_BV(pin);

      return snprintf_P(output, len, on ? PSTR("on") : PSTR("off"));
    } else 
      return snprintf_P(output, len, PSTR("error: pin is input"));

  } else
    return -1;
}
/* }}} */

static int16_t parse_cmd_pin_toggle(char *cmd, char *output, uint16_t len)
/* {{{ */ {
  uint16_t port, pin;

  /* Parse String */
  uint8_t ret = sscanf_P(cmd, PSTR("%u %u"), &port, &pin);
  /* Fallback to named pins */
  if ( ret != 2 && *cmd) {
    uint8_t pincfg = named_pin_by_name(cmd + 1);
    if (pincfg != 255) {
        port = pgm_read_byte(&portio_pincfg[pincfg].port);
        pin = pgm_read_byte(&portio_pincfg[pincfg].pin);
        ret = 2;
    }
  }
  if (ret == 2 && port < IO_PORTS && pin < 8) {
    /* Toggle only if it is output */
    if (cfg.options.io_ddr[port] & _BV(pin)) {
      uint8_t on = cfg.options.io[port] & _BV(pin);

      uint8_t pincfg = named_pin_by_pin(port, pin);
      uint8_t active_high = 1;
      if (pincfg != 255)  
        active_high = pgm_read_byte(&portio_pincfg[pincfg].active_high);

      if (on) 
        cfg.options.io[port] &= ~_BV(pin);
      else
        cfg.options.io[port] |= _BV(pin);

      return snprintf_P(output, len, XOR_LOG(!on, !active_high)
                        ? PSTR("on") : PSTR("off"));
    } else 
      return snprintf_P(output, len, PSTR("error: pin is input"));

  } else
    return -1;
}
/* }}} */
#endif

#ifdef HD44780_SUPPORT
static int16_t parse_lcd_clear(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    uint16_t line;

    int ret = sscanf_P(cmd,
            PSTR("%u"),
            &line);

    if (ret == 1) {
        if (line > 3)
            return -1;

        hd44780_goto(LO8(line), 0);
        for (uint8_t i = 0; i < 20; i++)
            fputc(' ', lcd);
        hd44780_goto(LO8(line), 0);

        return 0;
    } else {
        hd44780_clear();
        hd44780_goto(0, 0);
        return 0;
    }
} /* }}} */

static int16_t parse_lcd_write(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    if (strlen(cmd) > 1) {
        fputs(cmd+1, lcd);
        return 0;
    } else
        return -1;
} /* }}} */

static int16_t parse_lcd_goto(char *cmd, char *output, uint16_t len)
/* {{{ */ {
    uint16_t line, pos = 0;

    int ret = sscanf_P(cmd,
            PSTR("%u %u"),
            &line, & pos);

    if (ret >= 1 && line < 4) {
        if (ret == 2 && pos >= 20) {
            pos = 20;
        } else if (ret == 1)
            pos = 0;

        debug_printf("going to line %u, pos %u\n", line, pos);

        hd44780_goto(LO8(line), LO8(pos));
        return 0;
    } else
        return -1;

} /* }}} */
#endif

/* low level parsing functions */

/* parse an ip address at cmd, write result to ptr */
int8_t parse_ip(char *cmd, uint8_t *ptr)
/* {{{ */ {

#ifdef DEBUG_ECMD_IP
    debug_printf("called parse_ip with string '%s'\n", cmd);
#endif

    int *ip = __builtin_alloca(sizeof(int) * 4);

    /* return -2 if malloc() failed */
    if (ip == NULL)
        return -2;

    int ret = sscanf_P(cmd, PSTR("%u.%u.%u.%u"), ip, ip+1, ip+2, ip+3);

#ifdef DEBUG_ECMD_IP
    debug_printf("scanf returned %d\n", ret);
#endif

    if (ret == 4) {
#ifdef DEBUG_ECMD_IP
        debug_printf("read ip %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
#endif

        /* copy ip to destination */
        if (ptr != NULL)
            for (uint8_t i = 0; i < 4; i++)
                ptr[i] = ip[i];

        ret = 0;
    } else
        ret = -1;

    return ret;
} /* }}} */

/* parse an ethernet address at cmd, write result to ptr */
int8_t parse_mac(char *cmd, uint8_t *ptr)
/* {{{ */ {

#ifdef DEBUG_ECMD_MAC
    debug_printf("called parse_mac with string '%s'\n", cmd);
#endif

    int *mac = __builtin_alloca(sizeof(int) * 6);

    /* return -2 if malloc() failed */
    if (mac == NULL)
        return -2;

    int ret = sscanf_P(cmd, PSTR("%x:%x:%x:%x:%x:%x"), mac, mac+1, mac+2, mac+3, mac+4, mac+5);

#ifdef DEBUG_ECMD_MAC
    debug_printf("scanf returned %d\n", ret);
#endif

    if (ret == 6) {
#ifdef DEBUG_ECMD_MAC
        debug_printf("read mac %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif

        /* copy mac to destination */
        if (ptr != NULL)
            for (uint8_t i = 0; i < 6; i++)
                ptr[i] = mac[i];

        ret = 0;
    } else
        ret = -1;

    return ret;
} /* }}} */

/* parse an onewire rom address at cmd, write result to ptr */
int8_t parse_ow_rom(char *cmd, uint8_t *ptr)
/* {{{ */ {

#ifdef DEBUG_ECMD_OW_ROM
    debug_printf("called parse_ow_rom with string '%s'\n", cmd);
#endif

    /* check if enough bytes have been given */
    if (strlen(cmd) < 16) {
#ifdef DEBUG_ECMD_OW_ROM
        debug_printf("incomplete command\n");
#endif
        return -1;
    }

    char b[3];

    for (uint8_t i = 0; i < 8; i++) {
        memcpy(b, cmd, 2);
        cmd += 2;
        b[2] = '\0';
        uint16_t val;
        int16_t ret = sscanf_P(b, PSTR("%x"), &val);
        *ptr++ = LO8(val);
    }

    return 1;
} /* }}} */