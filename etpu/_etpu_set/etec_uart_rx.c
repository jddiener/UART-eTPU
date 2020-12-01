/****************************************************************************
 * Copyright (C) 2020 ASH WARE, Inc.
 ****************************************************************************/
/****************************************************************************
 * FILE NAME: etec_uart_rx.c                                                *
 * DESCRIPTION:                                                             *
 * This function uses 1 channel, or 2 if the RTS feature is enabled, to     *
 * implement a UART receiver.                                               *
 ****************************************************************************/

#include <ETpu_Std.h>
#include "etec_uart.h"

#pragma verify_version GE, "2.62E", "use ETEC version 2.62E or newer"

/* provide hint that channel frame base addr same on all chans touched by func */
#pragma same_channel_frame_base UART_RX

#pragma export_autodef_macro "ETPU_UART_RX_UPDATE_RTS_HSR", 1
#pragma export_autodef_macro "ETPU_UART_RX_INIT_TCR1_HSR", 2
#pragma export_autodef_macro "ETPU_UART_RX_INIT_TCR2_HSR", 4
#pragma export_autodef_macro "ETPU_UART_RX_SHUTDOWN_HSR", 7

#pragma export_autodef_macro "ETPU_UART_RX_FRAMING_ERROR", 0x01
#pragma export_autodef_macro "ETPU_UART_RX_PARITY_ERROR", 0x02

#pragma export_autodef_macro "ETPU_UART_FM0_PARITY_DISABLED", FM0_PARITY_DISABLED
#pragma export_autodef_macro "ETPU_UART_FM0_PARITY_ENABLED", FM0_PARITY_ENABLED


_eTPU_thread UART::Init_RX_TCR1(_eTPU_matches_disabled)
{
    channel.TBSA = TBS_M1C1GE;
    Common_RX_Init_fragment();
}

_eTPU_fragment UART::Common_RX_Init_fragment()
{
    channel.TBSA = TBSA_CLR_OBE;
    channel.PDCM = PDCM_SM_ST;
    channel.IPACA = IPAC_FALLING;
    channel.FLAG0 = 0; /* cleared, but not used currently */
    channel.LSR = LSR_CLEAR;
    channel.TDL = TDL_CLEAR;
    channel.MRLA = MRL_CLEAR;
    channel.MRLB = MRL_CLEAR;
    channel.MTD = MTD_ENABLE;
    
    /* clear FIFO to start */
    _rx_buffer_pop_p = _rx_buffer_push_p = _rx_buffer_start_p;

    /* init data mask */
    _rx_data_mask = (1 << _bit_count) - 1;
    
    /* initialize RTS pin if feature enabled */
    if (_rts_chan_num >= 0)
    {
        chan = _rts_chan_num;
        channel.TBSA = TBSA_CLR_OBE;
        channel.PIN = PIN_SET_LOW;
    }
    
}

_eTPU_thread UART::Init_RX_TCR2(_eTPU_matches_disabled)
{
    channel.TBSA = TBS_M2C2GE;
    Common_RX_Init_fragment();
}

_eTPU_thread UART::Shutdown_RX(_eTPU_matches_disabled)
{
    channel.IPACA = IPAC_NO_DETECT;
    channel.MRLE = MRLE_DISABLE;
    channel.LSR = LSR_CLEAR;
    channel.TDL = TDL_CLEAR;
    channel.MRLA = MRL_CLEAR;
    channel.MRLB = MRL_CLEAR;
    channel.MTD = MTD_DISABLE;
    if (_rts_chan_num >= 0)
    {
        chan = _rts_chan_num;
        channel.PIN = PIN_SET_HIGH;
    }
}

_eTPU_thread UART::DetectWord(_eTPU_matches_enabled)
{
    _rx_one_bit = 1;
    _rx_shift_register = 0;
    _rx_parity_calc = 0;
    _rx_running_bit_count = _bit_count;
    if (channel.FM0 == FM0_PARITY_ENABLED)
    {
        _rx_parity_calc = _parity_select;
        _rx_running_bit_count += 1;
    }
    erta += (_bit_time + (_bit_time >> 1));
    channel.ERWA = ERW_WRITE_ERT_TO_MATCH;
    channel.IPACA = IPAC_NO_DETECT;
    channel.TDL = TDL_CLEAR;
}

_eTPU_thread UART::DetectBit(_eTPU_matches_enabled)
{
    channel.MRLA = MRL_CLEAR;
    if (_rx_running_bit_count == 0)
    {
        uint8_t error_flags = 0;
        int24_t fifo_used_size;
        struct uart_rx_data_word_t* next_p, *pop_p;
        
        /* this is the stop bit, check it */
#ifdef __TARGET_ETPU2__
        if (prss == 0)
#else
        if (pss == 0)
#endif
        {
            error_flags |= FRAMING_ERROR;
        }
        if (channel.FM0 == FM0_PARITY_ENABLED && (_rx_parity_calc & 1) != 0)
        {
            error_flags |= PARITY_ERROR;
        }
        /* re-enable check for start bit */
        channel.IPACA = IPAC_FALLING;

        /* place data into FIFO, etc. */

        /* always put data in */
        _rx_buffer_push_p->_error_flags = error_flags;
        _rx_buffer_push_p->_data = _rx_shift_register & _rx_data_mask;

        /* was there room in FIFO? */
        /* error if not, otherwise increment push */
        next_p = _rx_buffer_push_p + 1;
        pop_p = _rx_buffer_pop_p; /* sample just once */
        if (next_p == _rx_buffer_end_p)
        {
            next_p = _rx_buffer_start_p;
        }
        if (next_p == pop_p)
        {
            _overrun_error = 1;
            /* just exit (data dropped) */
            return;
        }
        _rx_buffer_push_p = next_p;
        
        /* issue interrupt if threshold reached */
        fifo_used_size = (int24_t)next_p - (int24_t)pop_p;
        if (fifo_used_size < 0)
        {
            fifo_used_size += _rx_buffer_byte_size;
        }
        if (fifo_used_size == _rx_fifo_int_threshold)
        {
            channel.CIRC = CIRC_INT_FROM_SERVICED;
        }
        
        /* update RTS output if feature enabled and threshold crossed */
        if (_rts_chan_num >= 0)
        {
            chan = _rts_chan_num;
            if (fifo_used_size >= _rx_rts_halt_threshold)
            {
                channel.PIN = PIN_SET_HIGH;
            }
            else if (fifo_used_size <= _rx_rts_resume_threshold)
            {
                channel.PIN = PIN_SET_LOW;
            }
        }
    }
    else
    {
        _rx_running_bit_count -= 1;
#ifdef __TARGET_ETPU2__
        if (prss == 1)
#else
        if (pss == 1)
#endif
        {
            _rx_shift_register |= _rx_one_bit;
            _rx_parity_calc += 1;
        }
        _rx_one_bit <<= 1;
        erta = erta + _bit_time;
        channel.ERWA = ERW_WRITE_ERT_TO_MATCH;
    }
}

_eTPU_thread UART::UpdateRTS(_eTPU_matches_enabled)
{
    int24_t fifo_used_size;

    if (_rts_chan_num >= 0)
    {
        fifo_used_size = (int24_t)_rx_buffer_push_p - (int24_t)_rx_buffer_pop_p;
        if (fifo_used_size < 0)
        {
            fifo_used_size += _rx_buffer_byte_size;
        }
        chan = _rts_chan_num;
        if (fifo_used_size >= _rx_rts_halt_threshold)
        {
            channel.PIN = PIN_SET_HIGH;
        }
        else if (fifo_used_size <= _rx_rts_resume_threshold)
        {
            channel.PIN = PIN_SET_LOW;
        }
    }
}



DEFINE_ENTRY_TABLE(UART, UART_RX, standard, inputpin, autocfsr)
{
	//           HSR LSR M1 M2 PIN F0 F1 vector
	ETPU_VECTOR1(1,  x,  x, x, 0,  0, x, UpdateRTS),
	ETPU_VECTOR1(1,  x,  x, x, 0,  1, x, UpdateRTS),
	ETPU_VECTOR1(1,  x,  x, x, 1,  0, x, UpdateRTS),
	ETPU_VECTOR1(1,  x,  x, x, 1,  1, x, UpdateRTS),
	ETPU_VECTOR1(2,  x,  x, x, x,  x, x, Init_RX_TCR1),
	ETPU_VECTOR1(3,  x,  x, x, x,  x, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(4,  x,  x, x, x,  x, x, Init_RX_TCR2),
	ETPU_VECTOR1(5,  x,  x, x, x,  x, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(6,  x,  x, x, x,  x, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(7,  x,  x, x, x,  x, x, Shutdown_RX),
	ETPU_VECTOR1(0,  1,  1, 1, x,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  1, 1, x,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  0, 1, 0,  0, x, DetectWord),
	ETPU_VECTOR1(0,  0,  0, 1, 0,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  0, 1, 1,  0, x, DetectWord),
	ETPU_VECTOR1(0,  0,  0, 1, 1,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 0, 0,  0, x, DetectBit),
	ETPU_VECTOR1(0,  0,  1, 0, 0,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 0, 1,  0, x, DetectBit),
	ETPU_VECTOR1(0,  0,  1, 0, 1,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 1, 0,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 1, 0,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 1, 1,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 1, 1,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 0, 0,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 0, 0,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 0, 1,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 0, 1,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 1, x,  0, x, DetectWord),
	ETPU_VECTOR1(0,  1,  0, 1, x,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  1, 0, x,  0, x, DetectBit),
	ETPU_VECTOR1(0,  1,  1, 0, x,  1, x, _Error_handler_unexpected_thread),
};

