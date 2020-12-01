/****************************************************************************
 * Copyright (C) 2020 ASH WARE, Inc.
 ****************************************************************************/
/****************************************************************************
 * FILE NAME: etec_uart_tx.c                                                *
 * DESCRIPTION:                                                             *
 * This function uses 1 channel, or 2 if the CTS or 485 feature is enabled, *
 * to implement a UART transmitter.                                         *
 ****************************************************************************/

#include <ETpu_Std.h>
#include "etec_uart.h"

#pragma verify_version GE, "2.62E", "use ETEC version 2.62E or newer"

/* provide hint that channel frame base addr same on all chans touched by func */
#pragma same_channel_frame_base UART_TX

#pragma export_autodef_macro "ETPU_UART_TX_INIT_TCR1_HSR", 2
#pragma export_autodef_macro "ETPU_UART_TX_INIT_TCR2_HSR", 4
#pragma export_autodef_macro "ETPU_UART_TX_SHUTDOWN_HSR", 7


_eTPU_thread UART::Init_TX_TCR1(_eTPU_matches_disabled)
{
    channel.TBSA = TBS_M1C1GE;
    erta = tcr1;
    if (_tx_enable_chan_num >= 0)
    {
        uint8_t tmp = chan;
        chan = _tx_enable_chan_num;
        channel.TBSA = TBSA_SET_OBE;
        channel.PDCM = PDCM_SM_ST;
        channel.PIN = PIN_SET_LOW;
        channel.OPACA = OPAC_MATCH_LOW;
        channel.TBSA = TBS_M1C1GE;
        chan = tmp;
    }
    Common_TX_Init_fragment();
}

_eTPU_fragment UART::Common_TX_Init_fragment()
{
    channel.TBSA = TBSA_SET_OBE;
    channel.PDCM = PDCM_SM_ST;
    channel.OPACA = OPAC_NO_CHANGE;
    channel.PIN = PIN_SET_HIGH;
    channel.FLAG0 = 0;
    channel.LSR = LSR_CLEAR;
    channel.MRLA = MRL_CLEAR;
    channel.MRLB = MRL_CLEAR;
    channel.TDL = TDL_CLEAR;
    erta += _stop_time;
    channel.ERWA = ERW_WRITE_ERT_TO_MATCH;
    channel.MTD = MTD_ENABLE;

    /* clear FIFO to start */
    _tx_buffer_pop_p = _tx_buffer_push_p = _tx_buffer_start_p;
    
    if (_cts_chan_num >= 0)
    {
        chan = _cts_chan_num;
        channel.TBSA = TBSA_CLR_OBE;
    }    
}

_eTPU_thread UART::Init_TX_TCR2(_eTPU_matches_disabled)
{
    channel.TBSA = TBS_M2C2GE;
    erta = tcr2;
    if (_tx_enable_chan_num >= 0)
    {
        uint8_t tmp = chan;
        chan = _tx_enable_chan_num;
        channel.TBSA = TBSA_SET_OBE;
        channel.PDCM = PDCM_SM_ST;
        channel.PIN = PIN_SET_LOW;
        channel.OPACA = OPAC_MATCH_LOW;
        channel.TBSA = TBS_M2C2GE;
        chan = tmp;
    }
    Common_TX_Init_fragment();
}

_eTPU_thread UART::Shutdown_TX(_eTPU_matches_disabled)
{
    channel.MRLE = MRLE_DISABLE;
    channel.LSR = LSR_CLEAR;
    channel.TDL = TDL_CLEAR;
    channel.MRLA = MRL_CLEAR;
    channel.MRLB = MRL_CLEAR;
    channel.MTD = MTD_DISABLE;
    channel.PIN = PIN_SET_HIGH;
    if (_tx_enable_chan_num >= 0)
    {
        chan = _tx_enable_chan_num;
        channel.MRLE = MRLE_DISABLE;
        channel.PIN = PIN_SET_LOW;
    }
}

_eTPU_thread UART::TransmitCheck(_eTPU_matches_enabled)
{
    uint24_t* pop_p, * push_p;
    int24_t fifo_used_size;

    channel.MRLA = MRL_CLEAR;
    erta += _stop_time;
    channel.ERWA = ERW_WRITE_ERT_TO_MATCH;
    if ((pop_p = _tx_buffer_pop_p) != (push_p = _tx_buffer_push_p))
    {
        /* if CTS enabled and not active, do not send */
        if (_cts_chan_num >= 0)
        {
            uint8_t tmp = chan;
            chan = _cts_chan_num;
            if (channel.PSTI == 1)
            {
                /* not clear to send, must wait */
                /* perform TXE end processing as needed and exit */
                chan = tmp;
                FinishTXE_fragment();
            }
            chan = tmp;
        }
        
        /* if in 485 mode, update tx enable */
        if (_tx_enable_chan_num >= 0)
        {
            uint8_t tmp = chan;
            chan = _tx_enable_chan_num;
            channel.MRLE = MRLE_DISABLE;
            channel.PIN = PIN_SET_HIGH;
            _tx_enable_active = TRUE;
            chan = tmp;
        }
        
        channel.OPACA = OPAC_MATCH_LOW;
        channel.FLAG0 = 1;
        _tx_parity_calc = _parity_select;
        _tx_running_bit_count = _bit_count;
        _tx_shift_register = *_tx_buffer_pop_p;
        
        /* update pop ptr and interrupt host if necessary */
        pop_p += 1;
        if (pop_p == _tx_buffer_end_p)
        {
            pop_p = _tx_buffer_start_p;
        }
        _tx_buffer_pop_p = pop_p;
        
        fifo_used_size = (int24_t)push_p - (int24_t)pop_p;
        if (fifo_used_size < 0)
        {
            fifo_used_size += _tx_buffer_byte_size;
        }
        if (fifo_used_size == _tx_fifo_int_threshold)
        {
            channel.CIRC = CIRC_INT_FROM_SERVICED;
        }
    }
    else
    {
        FinishTXE_fragment();
    }
}

_eTPU_fragment UART::FinishTXE_fragment()
{
    if (_tx_enable_active == TRUE)
    {
        /* this is the end of an RS-485 mode transfer, set up end of TX enable */
        int24_t tmp = erta;
        _tx_enable_active = FALSE;
        chan = _tx_enable_chan_num;
        channel.MRLA = MRL_CLEAR;
        erta = tmp + _tx_enable_post_delay;
        channel.ERWA = ERW_WRITE_ERT_TO_MATCH;
    }
}

_eTPU_thread UART::TransmitBit(_eTPU_matches_enabled)
{
    if (_tx_running_bit_count == 0)
    {
        if (channel.FM0 == FM0_PARITY_DISABLED)
        {
            /* no parity; issue stop bit */
            channel.OPACA = OPAC_MATCH_HIGH;
            channel.FLAG0 = 0;
        }
        else
        {
            channel.OPACA = OPAC_MATCH_HIGH;
            if ((_tx_parity_calc & 1) == 0)
                channel.OPACA = OPAC_MATCH_LOW;
        }
    }
    else if (_tx_running_bit_count < 0)
    {
        /* issue stop bit */
        channel.OPACA = OPAC_MATCH_HIGH;
        channel.FLAG0 = 0;
    }
    else
    {
        channel.OPACA = OPAC_MATCH_LOW;
        if ((_tx_shift_register & 1) != 0)
        {
            channel.OPACA = OPAC_MATCH_HIGH;
            _tx_parity_calc += 1;
        }
    }
    _tx_running_bit_count -= 1;
    _tx_shift_register >>= 1;
    channel.MRLA = MRL_CLEAR;
    erta = erta + _bit_time;
    channel.ERWA = ERW_WRITE_ERT_TO_MATCH;
}



DEFINE_ENTRY_TABLE(UART, UART_TX, standard, outputpin, autocfsr)
{
	//           HSR LSR M1 M2 PIN F0 F1 vector
	ETPU_VECTOR1(1,  x,  x, x, 0,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(1,  x,  x, x, 0,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(1,  x,  x, x, 1,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(1,  x,  x, x, 1,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(2,  x,  x, x, x,  x, x, Init_TX_TCR1),
	ETPU_VECTOR1(3,  x,  x, x, x,  x, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(4,  x,  x, x, x,  x, x, Init_TX_TCR2),
	ETPU_VECTOR1(5,  x,  x, x, x,  x, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(6,  x,  x, x, x,  x, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(7,  x,  x, x, x,  x, x, Shutdown_TX),
	ETPU_VECTOR1(0,  1,  1, 1, x,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  1, 1, x,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  0, 1, 0,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  0, 1, 0,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  0, 1, 1,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  0, 1, 1,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 0, 0,  0, x, TransmitCheck),
	ETPU_VECTOR1(0,  0,  1, 0, 0,  1, x, TransmitBit),
	ETPU_VECTOR1(0,  0,  1, 0, 1,  0, x, TransmitCheck),
	ETPU_VECTOR1(0,  0,  1, 0, 1,  1, x, TransmitBit),
	ETPU_VECTOR1(0,  0,  1, 1, 0,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 1, 0,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 1, 1,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  0,  1, 1, 1,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 0, 0,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 0, 0,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 0, 1,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 0, 1,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 1, x,  0, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  0, 1, x,  1, x, _Error_handler_unexpected_thread),
	ETPU_VECTOR1(0,  1,  1, 0, x,  0, x, TransmitCheck),
	ETPU_VECTOR1(0,  1,  1, 0, x,  1, x, TransmitBit),
};

