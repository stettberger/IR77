/* vim:fdm=marker ts=4 et ai sts=4 sw=4
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

#include <stdlib.h>

#include "handler.h"
#include "../uip/uip.h"
#include "../crypto/skipjack.h"
#include "../crypto/cast5.h"

#include "ecmd_net.h"
#include "tetrirape_net.h"
#include "bootp_net.h"
#include "tftp_net.h"
#include "ecmd_sender_net.h"

void network_init_apps(void)
/* {{{ */ {

#   ifdef ECMD_SUPPORT
    ecmd_net_init();
#   endif

#   ifdef TETRIRAPE_SUPPORT
    tetrirape_net_init();
#   endif

#   ifdef BOOTP_SUPPORT
    bootp_net_init();
#   endif

#   ifdef TFTP_SUPPORT
    tftp_net_init();
#   endif

    /* initialize your applications here */

} /* }}} */


#ifdef TCP_SUPPORT 
void network_handle_tcp(void)
/* {{{ */ {

#ifdef DEBUG_NET
    uart_puts_P("net_tcp: local port is 0x");
    uart_puthexbyte(HI8(uip_conn->lport));
    uart_puthexbyte(LO8(uip_conn->lport));
    uart_eol();
#endif

#   ifdef RC4_SUPPORT
    if(uip_connected()) {
        /* new connection, initialize flags */
        uip_conn->rc4_flags.inbound_initialized = 0;
        uip_conn->rc4_flags.outbound_initialized = 0;
    }

    if(! uip_conn->rc4_flags.inbound_initialized && uip_newdata()) {
        if(uip_len < 8) {
            /* not enough bytes for initialization of inbound rc4
               keystream generator.  cannot recover.  */
            uip_abort();
            return;
        }

        rc4_init(&uip_conn->rc4_inbound, uip_appdata);
        uip_conn->rc4_flags.inbound_initialized = 1;

        /* remove iv, i.e. hide from application */
        if(uip_len > 8) {
            memmove(uip_appdata, uip_appdata + 8, uip_len - 8);
            uip_len -= 8;
        }
        else
            /* reset newdata flag for application. */
            uip_flags &= ~UIP_NEWDATA;
    }

    if(uip_rexmit()) {
        /* our outbound rc4 stream generator is out of sync, 
         * we cannot retransmit.  reset connection.  FIXME */
        uip_abort();
        return;
    }

    if(uip_newdata()) {
        /* new data for application, decrypt uip_len bytes from 
           uip_appdata on. */
        uint16_t i;
        for(i = 0; i < uip_len; i ++)
            ((char *) uip_appdata)[i] = 
              rc4_crypt_char(&uip_conn->rc4_inbound, ((char *) uip_appdata)[i]);
    }
#   endif /* RC4_SUPPORT */

#   ifdef AUTH_SUPPORT 
    if(uip_connected()) {
        uip_conn->auth_okay = 0;

        if(uip_newdata()) {
            /* We already got data, what's that, the peer should wait for
             * our challenge first, reset connection.  */
            uip_abort();
            return;
        }

        /* Generate challenge and store until reception of response. */
        uint8_t i;
        for(i = 0; i < 8; i ++)
            uip_conn->appstate.auth_challenge[i] = rand() % 0xFF;

        /* Send challenge to peer. */
        uip_send(uip_conn->appstate.auth_challenge, 8);

        /* Don't call the application, we'll lie about a new connection
         * later on. */
        goto auth_out;
    }

    if(! uip_conn->auth_okay) {
        /* The connection hasn't been authenticated so far, thusly don't
         * let anything through to the application itself.  */
        if(uip_rexmit()) {
            uip_send(uip_conn->appstate.auth_challenge, 8);
            goto auth_out;
        }

        if(uip_newdata()) {
            if(uip_len < 8) {
                uip_abort();
                return;
            }

#           if defined(SKIPJACK_SUPPORT)
            unsigned char key[10] = "ABCDEF2342";
            skipjack_enc(uip_conn->appstate.auth_challenge, key);

#           elif defined(CAST5_SUPPORT)
            unsigned char key[16] = "ABCDEF23ABCDEF23";
            cast5_ctx_t ctx;
            cast5_init(&ctx, key, 128);
            cast5_enc(&ctx, uip_conn->appstate.auth_challenge);

#           elif !defined(RC4_SUPPORT)
#           warn "performing (useless) tcp auth without encryption!"
#           endif

            if(memcmp(uip_conn->appstate.auth_challenge, uip_appdata, 8)) {
                /* The response doesn't match the encrypted challenge, 
                 * i.e. the peer didn't authenticate itself correctly,
                 * therefore simply reset the connection.  */
                uip_abort();
                return;
            }

            uip_conn->auth_okay = 1;

            /* strip received response from incoming data */
            uip_len -= 8;
            if(uip_len)
                memmove(uip_appdata, uip_appdata + 8, uip_len);
            else
                /* reset newdata flag for application. */
                uip_flags &= ~UIP_NEWDATA;

            /* Let's call the application the very first time, i.e.
             * lie about the connection being new.  */
            uip_flags |= UIP_CONNECTED;
        }
    }

#   endif /* AUTH_SUPPORT */


    /* 
     * demultiplex packet
     */
    if (uip_conn->callback != NULL) 
        uip_conn->callback();

    /* put tcp application calls here, example:
     *
     * if (uip_conn->lport == HTONS(ETHCMD_PORT))
     *     ethcmd_main();
     *
     * if (uip_conn->lport == HTONS(HTTPD_PORT) ||
     *     uip_conn->lport == HTONS(HTTPD_ALTERNATE_PORT))
     *         httpd_main();
     */

#   ifdef AUTH_SUPPORT 
    /* For challenge/response authentication to skip the application calls, if
     * used in combination with RC4, our challenge will be encapsulated. */
 auth_out:
    (void) 0;
     
#   endif /* AUTH_SUPPORT */

#   ifdef RC4_SUPPORT
    /* new data from application, 
       encrypt uip_slen bytes from uip_sappdata on. */
     
    if(! uip_conn->rc4_flags.outbound_initialized) {
        /* Outbound rc4 keystream generator hasn't been initialized so far,
           generate random iv, send to peer as unciphered data (prepend to
           data generated by application */
        if(uip_slen)
            /* we need to make room, before the other data */
            memmove(uip_sappdata + 8, uip_sappdata, uip_slen);

        uint8_t j;
        for(j = 0; j < 8; j ++) 
            ((char *) uip_sappdata)[j] = rand() & 0xFF;

        uip_slen += 8;
        rc4_init(&uip_conn->rc4_outbound, uip_sappdata);
    }

    uint16_t i;
    for(i = uip_conn->rc4_flags.outbound_initialized ? 0 : 8; 
        i < uip_slen; i ++)
         ((char *) uip_sappdata)[i] = 
           rc4_crypt_char(&uip_conn->rc4_outbound, 
                          ((char *) uip_sappdata)[i]);

    uip_conn->rc4_flags.outbound_initialized = 1;

#   endif /* RC4_SUPPORT */

} /* }}} */
#endif /* TCP_SUPPORT */


#ifdef UDP_SUPPORT
void network_handle_udp(void)
/* {{{ */ {
    if (uip_udp_conn->callback)
        uip_udp_conn->callback();

    /* put udp application calls here, example:
     *
     * if (uip_udp_conn->lport == HTONS(SNTP_UDP_PORT))
     *     sntp_handle_conn();
     * 
     * if (uip_udp_conn->lport == HTONS(SYSLOG_UDP_PORT))
     *     syslog_handle_conn();
     * 
     * if (uip_udp_conn->lport == HTONS(FC_UDP_PORT))
     *     fc_handle_conn();
     */

} /* }}} */
#endif /* UDP_SUPPORT */
