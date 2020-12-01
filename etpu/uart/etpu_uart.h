/**************************************************************************
 * FILE NAME: etpu_uart.h                                                 *
 * DESCRIPTION:                                                           *
 * This file contains the prototypes and defines for the eTPU UART        *
 * Host Interface.                                                        *
 *========================================================================*/

#ifndef __ETPU_UART_H
#define __ETPU_UART_H

#include "etpu_util_ext.h"
#include "etpu_auto_api.h"

#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************/
/*                            Definitions                                 */
/**************************************************************************/

#define ETPU_UART_PARITY_EVEN   0
#define ETPU_UART_PARITY_ODD    1
#define ETPU_UART_PARITY_NONE   2

/* note: additional macro defintions can be found in the etpu_set_defines.h file */

/* format for a received UART word includes a combination of error flags and data */
/* MSB_BITFIELD_ORDER compilation switch defined in etpu_auto_api.h */
#ifdef MSB_BITFIELD_ORDER
union uart_rx_data_t
{
    struct {
        uint32_t unused_error_flags : 6;
        uint32_t parity_error_flag : 1;
        uint32_t framing_error_flag : 1;
        uint32_t data : 24;
    } rx_data_parts;
    uint32_t rx_data_word;
};
#else
union uart_rx_data_t
{
    struct {
        uint32_t data : 24;
        uint32_t unused_error_flags : 6;
        uint32_t parity_error_flag : 1;
        uint32_t framing_error_flag : 1;
    } rx_data_parts;
    uint32_t rx_data_word;
};
#endif


/**************************************************************************/
/*                         Type Definitions                               */
/**************************************************************************/

/** A structure to represent an instance of a UART
 *  It includes static UART initialization items. */
struct uart_instance_t
{
    ETPU_MODULE   em;
    /* to disable any feature/channel, set to 0xff */
    /* 0-31 for eTPU-A or eTPU-C, 64-95 for eTPU-B */
    /* all channels must be on same eTPU engine */
    uint8_t       rx_chan_num;
    uint8_t       tx_chan_num;
    uint8_t       cts_chan_num;
    uint8_t       rts_chan_num;
    uint8_t       txe_chan_num;
    uint8_t       priority;
    void          *cpba;        /* set during initialization */
    void          *cpba_pse;    /* set during initialization */
    void          *rx_fifo_buffer; /* stores address of RX FIFO allocated during initialization */
    void          *tx_fifo_buffer; /* stores address of TX FIFO allocated during initialization */
};
/** A structure to represent a configuration of a UART.
 *  It includes configuration items which can be changed in run-time. */
struct uart_config_t
{
    uint8_t       timer; /* FS_ETPU_TCR1 or FS_ETPU_TCR2 */
    uint8_t       bit_count; /* 1-23 */
    uint8_t       parity_select;
    uint32_t      baud_rate_hz;
    uint32_t      stop_time_half_bit_count; /* specify stop time in number of half bit times */
    uint32_t      rx_fifo_word_size; /* size in data words to allocate for RX FIFO */
    uint32_t      tx_fifo_word_size; /* size in data words to allocate for TX FIFO */
    uint32_t      rx_fifo_interrupt_threshold; /* when RX FIFO this full or fuller, interrupt on each new word received */
    uint32_t      tx_fifo_interrupt_threshold; /* when TX FIFO this empty, or emptier, interrupt on each word transmit */
    
    /* hardware flow control */
    uint32_t      rts_halt_threshold; /* when RX FIFO this full, de-assert RTS */
    uint32_t      rts_resume_threshold; /* when RX FIFO this empty (or less), assert RTS */
    
    /* RS-485 TX enable */
    uint32_t      tx_enable_half_bit_count; /* post TX delay of TX enable output de-assert, in count of half bit times */
};


/**************************************************************************/
/*                       Function Prototypes                              */
/**************************************************************************/

/**************************************************************************
 * etpu_uart_init() - this routine initializes an eTPU UART driver optionally
 * consisting of RX/TX channels and hardware flow control channels (pins), per
 * the specified configuration data.
 *
 * p_uart_instance - pointer to a UART instance structure.
 *
 * p_uart_config - pointer to a UART configuration structure.
 *
 * Returns failure code, or pass (0).
 **************************************************************************/
int32_t etpu_uart_init(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config);

/**************************************************************************
 * etpu_uart_transmit_data() - this routine requests a data transfer for up 
 * to the specified number of bytes. The TX FIFO gets loaded with as many of
 * the specified data words as possible.
 *
 * p_uart_instance - pointer to a UART instance structure.
 *
 * p_uart_config - pointer to a UART configuration structure.
 *
 * p_data_buffer - pointer to the buffer of data to be transmitted.
 *
 * data_request_cnt - the requested number of data words to transfer (not
 * all may actually get pushed into TX FIFO if it runs out of room).
 *
 * Returns the number of data words pushed onto TX FIFO.
 **************************************************************************/
int32_t etpu_uart_transmit_data(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config,
    uint32_t               *p_data_buffer,
    int32_t                 data_request_cnt);

/**************************************************************************
 * etpu_uart_receive_data() - this routine requests to read data from the RX
 * FIFO, up to a specified number of words.
 *
 * p_uart_instance - pointer to a UART instance structure.
 *
 * p_uart_config - pointer to a UART configuration structure.
 *
 * p_data_buffer - pointer to the buffer into which to place data popped
 * off the RX FIFO.
 *
 * data_buffer_size - the maximum number of data words to be read.
 *
 * p_overrun_error_status - pointer to where to write the overrun error status.
 * Overrun occurs when the RX FIFO is full and a new word is received (and 
 * dropped). Ignored if 0/NULL.
 *
 * Returns the actual number of data words read from the RX FIFO.
 **************************************************************************/
int32_t etpu_uart_receive_data(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config,
    union uart_rx_data_t   *p_data_buffer,
    int32_t                 data_buffer_size,
    uint32_t               *p_overrun_error_status);

/**************************************************************************
 * etpu_uart_transmit_fifo_status() - this routine retrieves the status of
 * the TX FIFO - its size and the amount used, both in terms of data words.
 *
 * p_uart_instance - pointer to a UART instance structure.
 *
 * p_uart_config - pointer to a UART configuration structure.
 *
 * p_fifo_size - pointer to where to write FIFO size, or 0/NULL if info
 * not wanted.
 *
 * p_fifo_used - pointer to where to write FIFO used count, or 0/NULL if 
 * info not wanted.
 *
 * Returns failure code, or pass (0).
 **************************************************************************/
int32_t etpu_uart_transmit_fifo_status(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config,
    int32_t                *p_fifo_size,
    int32_t                *p_fifo_used);

/**************************************************************************
 * etpu_uart_receive_fifo_status() - this routine retrieves the status of
 * the RX FIFO - its size and the amount used, both in terms of data words.
 *
 * p_uart_instance - pointer to a UART instance structure.
 *
 * p_uart_config - pointer to a UART configuration structure.
 *
 * p_fifo_size - pointer to where to write FIFO size, or 0/NULL if info
 * not wanted.
 *
 * p_fifo_used - pointer to where to write FIFO used count, or 0/NULL if 
 * info not wanted.
 *
 * Returns failure code, or pass (0).
 **************************************************************************/
int32_t etpu_uart_receive_fifo_status(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config,
    int32_t                *p_fifo_size,
    int32_t                *p_fifo_used);

#ifdef __cplusplus
}
#endif

#endif /* __ETPU_UART_H */
