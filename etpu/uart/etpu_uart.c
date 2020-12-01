/**************************************************************************
 * FILE NAME: etpu_uart.c                                                 *
 * DESCRIPTION:                                                           *
 * This file contains the ETPU UART API.                                  *
 **************************************************************************/

#include "etpu_util_ext.h"      /* Utility routines for working eTPU */
#include "etpu_auto_api.h"      /* auto-generated eTPU interface data */
#include "etpu_uart.h"          /* eTPU UART API header */


int32_t etpu_uart_init(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config)
{
    volatile struct eTPU_struct * eTPU;
    uint32_t timer_freq;
    uint8_t init_chan_num = 0xff;
    uint32_t bit_time;

    if (p_uart_instance->em == EM_AB)
    {
        eTPU = eTPU_AB;
        if (p_uart_config->timer == FS_ETPU_TCR1)
        {
            if (p_uart_instance->rx_chan_num < 32 || p_uart_instance->tx_chan_num < 32)
            {
                timer_freq = etpu_a_tcr1_freq;
            }
            else
            {
                timer_freq = etpu_b_tcr1_freq;
            }
        }
        else
        {
            if (p_uart_instance->rx_chan_num < 32 || p_uart_instance->tx_chan_num < 32)
            {
                timer_freq = etpu_a_tcr2_freq;
            }
            else
            {
                timer_freq = etpu_b_tcr2_freq;
            }
        }
    }
    else
    {
        eTPU = eTPU_C;
        if (p_uart_config->timer == FS_ETPU_TCR1)
        {
            timer_freq = etpu_c_tcr1_freq;
        }
        else
        {
            timer_freq = etpu_c_tcr2_freq;
        }
    }

    /* first disable channels */
    if (p_uart_instance->rx_chan_num != 0xff)
    {
        fs_etpu_disable_ext(p_uart_instance->em, p_uart_instance->rx_chan_num);
        init_chan_num = p_uart_instance->rx_chan_num;
    }
    if (p_uart_instance->tx_chan_num != 0xff)
    {
        fs_etpu_disable_ext(p_uart_instance->em, p_uart_instance->tx_chan_num);
        init_chan_num = p_uart_instance->tx_chan_num;
    }
    if (p_uart_instance->cts_chan_num != 0xff)
        fs_etpu_disable_ext(p_uart_instance->em, p_uart_instance->cts_chan_num);
    if (p_uart_instance->rts_chan_num != 0xff)
        fs_etpu_disable_ext(p_uart_instance->em, p_uart_instance->rts_chan_num);
    if (p_uart_instance->txe_chan_num != 0xff)
        fs_etpu_disable_ext(p_uart_instance->em, p_uart_instance->txe_chan_num);
        
    /* at least one of RX/TX channels must be active */
    if (init_chan_num == 0xff)
        return FS_ETPU_ERROR_VALUE;

    /* get channel frame memory configured */
    if (eTPU->CHAN[init_chan_num].CR.B.CPBA == 0)
    {
        /* get parameter RAM for channel frame */
        p_uart_instance->cpba = fs_etpu_malloc_ext(p_uart_instance->em, _FRAME_SIZE_UART_);
        p_uart_instance->cpba_pse = (void*)((uint32_t)p_uart_instance->cpba + (fs_etpu_data_ram_ext - fs_etpu_data_ram_start));

        if (p_uart_instance->cpba  == 0)
        {
            return (FS_ETPU_ERROR_MALLOC);
        }
        
        /* allocate RX and TX FIFOs */
        if (p_uart_config->rx_fifo_word_size > 0)
        {
            p_uart_instance->rx_fifo_buffer = fs_etpu_malloc_ext(p_uart_instance->em, p_uart_config->rx_fifo_word_size * 4);
            if (p_uart_instance->rx_fifo_buffer == 0)
                return FS_ETPU_ERROR_MALLOC;
        }
        if (p_uart_config->tx_fifo_word_size > 0)
        {
            p_uart_instance->tx_fifo_buffer = fs_etpu_malloc_ext(p_uart_instance->em, p_uart_config->tx_fifo_word_size * 4);
            if (p_uart_instance->tx_fifo_buffer == 0)
                return FS_ETPU_ERROR_MALLOC;
        }
    }
    else  /* set cpba to what is in the CR register */
    {
        p_uart_instance->cpba = fs_etpu_get_cpba_ext(p_uart_instance->em, init_chan_num);
        p_uart_instance->cpba_pse = fs_etpu_get_cpba_pse_ext(p_uart_instance->em, init_chan_num);
        /* assume RX and TX buffers already initialized if in this case (can't change their size on re-init) */
    }

    /* intialize channel frame */
    fs_memset32_ext(p_uart_instance->cpba, 0, _FRAME_SIZE_UART_);
    ((etpu_if_UART_CHANNEL_FRAME*)p_uart_instance->cpba)->_bit_count = p_uart_config->bit_count;
    ((etpu_if_UART_CHANNEL_FRAME*)p_uart_instance->cpba)->_parity_select = p_uart_config->parity_select;
    ((etpu_if_UART_CHANNEL_FRAME*)p_uart_instance->cpba)->_cts_chan_num = p_uart_instance->cts_chan_num;
    ((etpu_if_UART_CHANNEL_FRAME*)p_uart_instance->cpba)->_rts_chan_num = p_uart_instance->rts_chan_num;
    ((etpu_if_UART_CHANNEL_FRAME*)p_uart_instance->cpba)->_tx_enable_chan_num = p_uart_instance->txe_chan_num;
    bit_time = timer_freq / p_uart_config->baud_rate_hz;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_bit_time = bit_time;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_stop_time = bit_time * p_uart_config->stop_time_half_bit_count / 2;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_enable_post_delay = bit_time * p_uart_config->tx_enable_half_bit_count / 2;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_byte_size = p_uart_config->rx_fifo_word_size * 4;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_start_p = (uint32_t)p_uart_instance->rx_fifo_buffer & 0x3fff;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_end_p = 
        ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_start_p + p_uart_config->rx_fifo_word_size * 4;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_byte_size = p_uart_config->tx_fifo_word_size * 4;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_start_p = (uint32_t)p_uart_instance->tx_fifo_buffer & 0x3fff;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_end_p = 
        ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_start_p + p_uart_config->tx_fifo_word_size * 4;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_fifo_int_threshold = p_uart_config->rx_fifo_interrupt_threshold * 4;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_fifo_int_threshold = p_uart_config->tx_fifo_interrupt_threshold * 4;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_rts_halt_threshold = p_uart_config->rts_halt_threshold * 4;
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_rts_resume_threshold = p_uart_config->rts_resume_threshold * 4;

    /* function mode */
    if (p_uart_config->parity_select < ETPU_UART_PARITY_NONE)
    {
        if (p_uart_instance->rx_chan_num != 0xff)
    	   eTPU->CHAN[p_uart_instance->rx_chan_num].SCR.R = ETPU_UART_FM0_PARITY_ENABLED;
        if (p_uart_instance->tx_chan_num != 0xff)
    	   eTPU->CHAN[p_uart_instance->tx_chan_num].SCR.R = ETPU_UART_FM0_PARITY_ENABLED;
    }
    else
    {
        if (p_uart_instance->rx_chan_num != 0xff)
    	   eTPU->CHAN[p_uart_instance->rx_chan_num].SCR.R = ETPU_UART_FM0_PARITY_DISABLED;
        if (p_uart_instance->tx_chan_num != 0xff)
    	   eTPU->CHAN[p_uart_instance->tx_chan_num].SCR.R = ETPU_UART_FM0_PARITY_DISABLED;
    }    

    /* write hsr */
    if (p_uart_config->timer == FS_ETPU_TCR1)
    {
        if (p_uart_instance->rx_chan_num != 0xff)
            eTPU->CHAN[p_uart_instance->rx_chan_num].HSRR.R = ETPU_UART_RX_INIT_TCR1_HSR;
        if (p_uart_instance->tx_chan_num != 0xff)
            eTPU->CHAN[p_uart_instance->tx_chan_num].HSRR.R = ETPU_UART_TX_INIT_TCR1_HSR;
    }
    else
    {
        if (p_uart_instance->rx_chan_num != 0xff)
            eTPU->CHAN[p_uart_instance->rx_chan_num].HSRR.R = ETPU_UART_RX_INIT_TCR2_HSR;
        if (p_uart_instance->tx_chan_num != 0xff)
            eTPU->CHAN[p_uart_instance->tx_chan_num].HSRR.R = ETPU_UART_TX_INIT_TCR2_HSR;
    }

    /* final channel configuration */
    /* CTS, RTS, TXE channels have same base address if enabled */
    if (p_uart_instance->cts_chan_num != 0xff)
        eTPU->CHAN[p_uart_instance->cts_chan_num].CR.R = (uint32_t) (((uint32_t)p_uart_instance->cpba & 0x3fff) >> 3);
    if (p_uart_instance->rts_chan_num != 0xff)
        eTPU->CHAN[p_uart_instance->rts_chan_num].CR.R = (uint32_t) (((uint32_t)p_uart_instance->cpba & 0x3fff) >> 3);
    if (p_uart_instance->txe_chan_num != 0xff)
        eTPU->CHAN[p_uart_instance->txe_chan_num].CR.R = (uint32_t) (((uint32_t)p_uart_instance->cpba & 0x3fff) >> 3);

    if (p_uart_instance->rx_chan_num != 0xff)
        eTPU->CHAN[p_uart_instance->rx_chan_num].CR.R =
            (p_uart_instance->priority << 28) + 
            (_ENTRY_TABLE_PIN_DIR_UART_UART_RX_ << 25) +
            (_ENTRY_TABLE_TYPE_UART_UART_RX_ << 24) +
            (_FUNCTION_NUM_UART_UART_RX_ << 16) + 
            (uint32_t) (((uint32_t)p_uart_instance->cpba & 0x3fff) >> 3);
    if (p_uart_instance->tx_chan_num != 0xff)
        eTPU->CHAN[p_uart_instance->tx_chan_num].CR.R =
            (p_uart_instance->priority << 28) + 
            (_ENTRY_TABLE_PIN_DIR_UART_UART_TX_ << 25) +
            (_ENTRY_TABLE_TYPE_UART_UART_TX_ << 24) +
            (_FUNCTION_NUM_UART_UART_TX_ << 16) + 
            (uint32_t) (((uint32_t)p_uart_instance->cpba & 0x3fff) >> 3);

    return 0;
}

int32_t etpu_uart_transmit_data(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config,
    uint32_t               *p_data_buffer,
    int32_t                 data_request_cnt)
{
    int32_t i;
    int32_t pop_index, push_index;
    int32_t words_used, words_available, words_written;
    uint32_t* push_addr, *end_addr;
    
    pop_index = (int32_t)(((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_pop_p - 
        ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_start_p) >> 2;
    push_index = (int32_t)(((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_push_p - 
        ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_start_p) >> 2;
    words_used = push_index - pop_index;
    if (words_used < 0)
        words_used = p_uart_config->tx_fifo_word_size + words_used;
    words_available = p_uart_config->tx_fifo_word_size - words_used - 1; /* FIFO full == size - 1 */
    words_written = (data_request_cnt < words_available) ? data_request_cnt : words_available;
    /* push data onto FIFO */
    push_addr = (uint32_t*)p_uart_instance->tx_fifo_buffer + push_index;
    end_addr = (uint32_t*)p_uart_instance->tx_fifo_buffer + p_uart_config->tx_fifo_word_size;
    for (i = 0 ; i < words_written; i++)
    {
        *push_addr++ = p_data_buffer[i];
        if (push_addr == end_addr)
            push_addr = (uint32_t*)p_uart_instance->tx_fifo_buffer;
    }
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_push_p = (uint32_t)push_addr & 0x3fff;

    return words_written;
}

int32_t etpu_uart_receive_data(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config,
    union uart_rx_data_t   *p_data_buffer,
    int32_t                 data_buffer_size,
    uint32_t               *p_overrun_error_status)
{
    volatile struct eTPU_struct * eTPU;
    int32_t read_cnt = 0;
    int32_t pop_index, push_index;
    uint32_t *push_addr, *pop_addr, *end_addr;

    if (p_uart_instance->em == EM_AB)
        eTPU = eTPU_AB;
    else
        eTPU = eTPU_C;

    pop_index = (int32_t)(((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_pop_p - 
        ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_start_p) >> 2;
    push_index = (int32_t)(((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_push_p - 
        ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_start_p) >> 2;
    pop_addr = (uint32_t*)p_uart_instance->rx_fifo_buffer + pop_index;
    push_addr = (uint32_t*)p_uart_instance->rx_fifo_buffer + push_index;
    end_addr = (uint32_t*)p_uart_instance->rx_fifo_buffer + p_uart_config->rx_fifo_word_size;
    /* pop data from FIFO */
    while (pop_addr != push_addr && read_cnt < data_buffer_size)
    {
        p_data_buffer[read_cnt++].rx_data_word = *pop_addr++;
        if (pop_addr == end_addr)
            pop_addr = (uint32_t*)p_uart_instance->rx_fifo_buffer;
    }
    ((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_pop_p = (uint32_t)pop_addr & 0x3fff;
    if (p_uart_instance->rx_chan_num != 0xff)
        eTPU->CHAN[p_uart_instance->rx_chan_num].HSRR.R = ETPU_UART_RX_UPDATE_RTS_HSR;
    
    /* get overrun error status if requested (clear if read) */
    if (p_overrun_error_status != 0)
    {
        *p_overrun_error_status = (uint32_t)((etpu_if_UART_CHANNEL_FRAME*)p_uart_instance->cpba)->_overrun_error;
        if (*p_overrun_error_status != 0)
            ((etpu_if_UART_CHANNEL_FRAME*)p_uart_instance->cpba)->_overrun_error = 0; /* clear it */
    }
    
    return read_cnt;
}

int32_t etpu_uart_transmit_fifo_status(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config,
    int32_t                *p_fifo_size,
    int32_t                *p_fifo_used)
{
    int32_t pop_index, push_index;
    int32_t words_used;
    pop_index = (int32_t)((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_pop_p >> 2;
    push_index = (int32_t)((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_tx_buffer_push_p >> 2;
    words_used = push_index - pop_index;
    if (words_used < 0)
        words_used = p_uart_config->tx_fifo_word_size + words_used;
    if (p_fifo_size != 0) *p_fifo_size = p_uart_config->tx_fifo_word_size;
    if (p_fifo_used != 0) *p_fifo_used = words_used;
    return 0;
}

int32_t etpu_uart_receive_fifo_status(
    struct uart_instance_t *p_uart_instance,
    struct uart_config_t   *p_uart_config,
    int32_t                *p_fifo_size,
    int32_t                *p_fifo_used)
{
    int32_t pop_index, push_index;
    int32_t words_used;
    pop_index = (int32_t)((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_pop_p >> 2;
    push_index = (int32_t)((etpu_if_UART_CHANNEL_FRAME_PSE*)p_uart_instance->cpba_pse)->_rx_buffer_push_p >> 2;
    words_used = push_index - pop_index;
    if (words_used < 0)
        words_used = p_uart_config->rx_fifo_word_size + words_used;
    if (p_fifo_size != 0) *p_fifo_size = p_uart_config->rx_fifo_word_size;
    if (p_fifo_used != 0) *p_fifo_used = words_used;
    return 0;
}
