// File 'etec_uart.h'

#define FRAMING_ERROR 0x01
#define PARITY_ERROR 0x02

#define FM0_PARITY_DISABLED 0
#define FM0_PARITY_ENABLED  1

struct uart_rx_data_word_t
{
    uint8_t _error_flags;
    uint24_t _data;
};


_eTPU_class UART
{
    /* channel frame */
public:
    int8_t _bit_count;
    int24_t _bit_time;
    int24_t _stop_time;

    uint8_t _parity_select; // 0=even, 1=odd, enabled when FM0=1, otherwise parity disabled if FM0=0
    
    uint8_t _overrun_error;
    
    /* FIFO control */
    int24_t _rx_buffer_byte_size;
    struct uart_rx_data_word_t* _rx_buffer_start_p;
    struct uart_rx_data_word_t* _rx_buffer_end_p;
    struct uart_rx_data_word_t* _rx_buffer_pop_p;
    struct uart_rx_data_word_t* _rx_buffer_push_p;
    
    int24_t _rx_rts_halt_threshold;
    int24_t _rx_rts_resume_threshold;

    int24_t _tx_buffer_byte_size;
    uint24_t* _tx_buffer_start_p;
    uint24_t* _tx_buffer_end_p;
    uint24_t* _tx_buffer_pop_p;
    uint24_t* _tx_buffer_push_p;

    int24_t _rx_fifo_int_threshold;
    int24_t _tx_fifo_int_threshold;

    /* hardware flow control */
    int8_t _cts_chan_num;
    int8_t _rts_chan_num;
   
    /* RS-485 support */
    int8_t _tx_enable_chan_num;
    int24_t _tx_enable_post_delay;
    
private:
    uint24_t _rx_shift_register;
    uint24_t _tx_shift_register;
    int8_t _rx_running_bit_count;
    int8_t _tx_running_bit_count;
    uint8_t _rx_parity_calc;
    uint24_t _rx_one_bit;
    uint24_t _rx_data_mask;
    
    uint8_t _tx_parity_calc;
    _Bool _tx_enable_active;


    /* threads */

    /* initialization */
    _eTPU_thread Init_RX_TCR1(_eTPU_matches_disabled);
    _eTPU_thread Init_RX_TCR2(_eTPU_matches_disabled);
    _eTPU_thread Init_TX_TCR1(_eTPU_matches_disabled);
    _eTPU_thread Init_TX_TCR2(_eTPU_matches_disabled);
    /* shutdown */
    _eTPU_thread Shutdown_RX(_eTPU_matches_disabled);
    _eTPU_thread Shutdown_TX(_eTPU_matches_disabled);

    /* RX threads */
    _eTPU_thread DetectWord(_eTPU_matches_enabled);
    _eTPU_thread DetectBit(_eTPU_matches_enabled);
    _eTPU_thread UpdateRTS(_eTPU_matches_enabled);

    /* TX threads */
    _eTPU_thread TransmitCheck(_eTPU_matches_enabled);
    _eTPU_thread TransmitBit(_eTPU_matches_enabled);
    
    /* fragments */
    _eTPU_fragment Common_RX_Init_fragment();
    _eTPU_fragment Common_TX_Init_fragment();
    _eTPU_fragment FinishTXE_fragment();
    
    /* methods */
    /* none */

    /* entry table(s) */
    _eTPU_entry_table UART_RX;    
    _eTPU_entry_table UART_TX;
};
