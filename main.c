/* main.c
 *
 * This is the entry point for the system.  System init
 * is performed, then the main app is kicked off.
 */

/* This code initializes the eTPU and a SPI master and slave that communicate with each
   other via connected pins.  A number of data transfers are done in all the different
   modes to perform a basic operational test. */

/* for sim environment */
#include "isrLib.h"
#include "ScriptLib.h"

/* for eTPU/SPI */
#include "etpu_util_ext.h"
#include "etpu_gct.h"
#include "etpu_uart.h"


int32_t g_complete_flag = 0;

uint32_t g_uart_1_tx_data[256];
uint32_t g_uart_2_tx_data[32];

#define UART_1_RX_BUFFER_SIZE 100
#define UART_2_RX_BUFFER_SIZE 40
union uart_rx_data_t g_uart_1_rx_data[UART_1_RX_BUFFER_SIZE];
union uart_rx_data_t g_uart_2_rx_data[UART_2_RX_BUFFER_SIZE];

#define UART_1_TRANSMIT_CHUNK_SIZE 47
#define UART_2_TRANSMIT_CHUNK_SIZE 10

uint32_t g_uart_1_rx_word_count = 0;
uint32_t g_uart_2_rx_word_count = 0;

uint32_t g_uart_1_rx_isr_counter = 0;
uint32_t g_uart_1_tx_isr_counter = 0;
uint32_t g_uart_2_rx_isr_counter = 0;
uint32_t g_uart_2_tx_isr_counter = 0;


void uart_1_rx_isr_handler(int32_t chan_num, uint32_t isr_mask)
{
    /* clear handled interrupt */
    eTPU_AB->CISR_A.R = isr_mask;
    g_uart_1_rx_isr_counter++;
    
    /* read data */
    g_uart_1_rx_word_count += etpu_uart_receive_data(&uart_1_instance, &uart_1_config, g_uart_1_rx_data, UART_1_RX_BUFFER_SIZE, 0);
}

void uart_1_tx_isr_handler(int32_t chan_num, uint32_t isr_mask)
{
    /* clear handled interrupt */
    eTPU_AB->CISR_A.R = isr_mask;
    g_uart_1_tx_isr_counter++;

    /* write some data */
    etpu_uart_transmit_data(&uart_1_instance, &uart_1_config, g_uart_1_tx_data, UART_1_TRANSMIT_CHUNK_SIZE);
}

void uart_2_rx_isr_handler(int32_t chan_num, uint32_t isr_mask)
{
    /* clear handled interrupt */
    eTPU_AB->CISR_A.R = isr_mask;
    g_uart_2_rx_isr_counter++;
    
    /* read data */
    g_uart_2_rx_word_count += etpu_uart_receive_data(&uart_2_instance, &uart_2_config, g_uart_2_rx_data, UART_2_RX_BUFFER_SIZE, 0);
}

void uart_2_tx_isr_handler(int32_t chan_num, uint32_t isr_mask)
{
    /* clear handled interrupt */
    eTPU_AB->CISR_A.R = isr_mask;
    g_uart_2_tx_isr_counter++;

    /* write some data */
    etpu_uart_transmit_data(&uart_2_instance, &uart_2_config, g_uart_2_tx_data, UART_2_TRANSMIT_CHUNK_SIZE);
}

uint32_t g_fail_loop_cnt = 0;
void fail_loop()
{
    g_fail_loop_cnt++;
}


/* main application entry point */
/* w/ GNU, if we name this main, it requires linking with the libgcc.a
   run-time support.  This may be useful with C++ because this extra
   code initializes static C++ objects.  However, this C demo will
   skip it */
int user_main()
{
    uint32_t i, j;
    double current_time, previous_time;
    uint32_t uart_1_rx_word_cnt_last = 0;
    uint32_t uart_2_rx_word_cnt_last = 0;
    uint32_t uart_1_rx_isr_counter_last = 0;
    uint32_t uart_2_rx_isr_counter_last = 0;
    int32_t words_read, words_written;
    uint32_t overrun_error_status;
    uint32_t tx_send_index, rx_read_index;
    uint32_t tmp_tx_buf[16];

//    uint32_t err_code;
//    uint32_t master_data, slave_data;
#if 0
    uint32_t cisr;
    uint32_t master_sclk_cisr_mask = 1 << (ETPU_SPI_MASTER1_SCLK_CHAN & 0x1f);
    uint32_t slave_sclk_cisr_mask = 1 << (ETPU_SPI_SLAVE1_SCLK_CHAN & 0x1f);
    uint32_t slave_ss_cisr_mask = 1 << (ETPU_SPI_SLAVE1_SS_CHAN & 0x1f);
#endif

	/* initialize interrupt support */
	isrLibInit();
	/* enable interrupt acknowledgement */
	isrEnableAllInterrupts();
	
    /* initialize eTPU */	
    if (my_system_etpu_init())
        return 1;

    /* start eTPU */
    my_system_etpu_start();

    /* connect interrupt handlers */
    isrConnect(
        ETPU_UART_1_RX_CHAN, 
        uart_1_rx_isr_handler, 
        (ETPU_UART_1_RX_CHAN & 0x1f),
        (1 << (ETPU_UART_1_RX_CHAN & 0x1f)));
    isrConnect(
        ETPU_UART_1_TX_CHAN, 
        uart_1_tx_isr_handler, 
        (ETPU_UART_1_TX_CHAN & 0x1f),
        (1 << (ETPU_UART_1_TX_CHAN & 0x1f)));
    isrConnect(
        ETPU_UART_2_RX_CHAN, 
        uart_2_rx_isr_handler, 
        (ETPU_UART_2_RX_CHAN & 0x1f),
        (1 << (ETPU_UART_2_RX_CHAN & 0x1f)));
    isrConnect(
        ETPU_UART_2_TX_CHAN, 
        uart_2_tx_isr_handler, 
        (ETPU_UART_2_TX_CHAN & 0x1f),
        (1 << (ETPU_UART_2_TX_CHAN & 0x1f)));

    /* enable channel interrupts for the RX/TX chans */
    fs_etpu_set_interrupt_mask_a_ext(uart_1_instance.em, ETPU_CIE_A);

    /* initialize the transmit buffers */
    for (i = 0; i < 256; i++)
    {
        g_uart_1_tx_data[i] = ((255 - i) << 8) | i;
    }
    for (i = 0; i < 32; i++)
    {
        g_uart_2_tx_data[i] = i+0x40;
    }

    /* kick off things with an initial transmit */
    etpu_uart_transmit_data(&uart_1_instance, &uart_1_config, g_uart_1_tx_data, UART_1_TRANSMIT_CHUNK_SIZE);
    etpu_uart_transmit_data(&uart_2_instance, &uart_2_config, g_uart_2_tx_data, UART_2_TRANSMIT_CHUNK_SIZE);


    while (1)
    {
        if (g_uart_1_rx_isr_counter != uart_1_rx_isr_counter_last)
        {
            /* check that received data is correct */
            uint32_t i;
            uint32_t new_word_cnt = g_uart_1_rx_word_count - uart_1_rx_word_cnt_last;
            uint32_t tx_data_index = uart_1_rx_word_cnt_last % UART_1_TRANSMIT_CHUNK_SIZE;
            
            for (i = 0; i < new_word_cnt; i++)
            {
                if (g_uart_1_rx_data[i].rx_data_word != g_uart_1_tx_data[tx_data_index])
                {
                    print("error: UART 1, RX data does not match TX data");
                    fail_loop();
                }
                
                if (++tx_data_index == UART_1_TRANSMIT_CHUNK_SIZE)
                    tx_data_index = 0;
            }
            
            uart_1_rx_isr_counter_last = g_uart_1_rx_isr_counter;
            uart_1_rx_word_cnt_last = g_uart_1_rx_word_count;
        }
        if (g_uart_2_rx_isr_counter != uart_2_rx_isr_counter_last)
        {
            /* check that received data is correct */
            uint32_t i;
            uint32_t new_word_cnt = g_uart_2_rx_word_count - uart_2_rx_word_cnt_last;
            uint32_t tx_data_index = uart_2_rx_word_cnt_last % UART_2_TRANSMIT_CHUNK_SIZE;
            
            for (i = 0; i < new_word_cnt; i++)
            {
                if (g_uart_2_rx_data[i].rx_data_word != g_uart_2_tx_data[tx_data_index])
                {
                    print("error: UART 2, RX data does not match TX data");
                    fail_loop();
                }
                
                if (++tx_data_index == UART_2_TRANSMIT_CHUNK_SIZE)
                    tx_data_index = 0;
            }
            
            uart_2_rx_isr_counter_last = g_uart_2_rx_isr_counter;
            uart_2_rx_word_cnt_last = g_uart_2_rx_word_count;
        }
        
        current_time = read_time();
        if (current_time > 20000.0) break;
    }

    /* disable channel interrupts for the RX/TX chans */
    fs_etpu_set_interrupt_mask_a_ext(uart_1_instance.em, 0);
    
    /* finish and clear any transmissions in progress */
    g_uart_1_rx_word_count += etpu_uart_receive_data(&uart_1_instance, &uart_1_config, g_uart_1_rx_data, UART_1_RX_BUFFER_SIZE, 0);
    g_uart_2_rx_word_count += etpu_uart_receive_data(&uart_2_instance, &uart_2_config, g_uart_2_rx_data, UART_2_RX_BUFFER_SIZE, 0);
    at_time(25000);
    g_uart_1_rx_word_count += etpu_uart_receive_data(&uart_1_instance, &uart_1_config, g_uart_1_rx_data, UART_1_RX_BUFFER_SIZE, 0);
    g_uart_2_rx_word_count += etpu_uart_receive_data(&uart_2_instance, &uart_2_config, g_uart_2_rx_data, UART_2_RX_BUFFER_SIZE, 0);

    at_time(30000);    

    /* trigger RX overrun on UART 1 */
    i = 0;
    while (i < 3)
    {
        int32_t fifo_used;
        etpu_uart_transmit_fifo_status(&uart_1_instance, &uart_1_config, 0, &fifo_used);
        if (fifo_used == 0)
        {
            etpu_uart_transmit_data(&uart_1_instance, &uart_1_config, g_uart_1_tx_data + 200, 50);
            i++;
        }
    }
    words_read = etpu_uart_receive_data(&uart_1_instance, &uart_1_config, g_uart_1_rx_data, UART_1_RX_BUFFER_SIZE, &overrun_error_status);
    if (overrun_error_status != 1)
    {
        print("error: overrun error expected");
        fail_loop();
    }
    for (i = 0; i < words_read; i++)
    {
        if (g_uart_1_rx_data[i].rx_data_word != g_uart_1_tx_data[200 + i%50])
        {
            print("error: UART 1, RX data does not match TX data");
            fail_loop();
        }
    }
    
    /* trigger hw flow control on UART 2 */
    at_time(43000);
    j = 0;
    tx_send_index = 0;
    rx_read_index = 0;
    words_written = etpu_uart_transmit_data(&uart_2_instance, &uart_2_config, g_uart_2_tx_data, 16);
    tx_send_index = (tx_send_index + words_written) % 32;
    at_time(45000);
    previous_time = read_time();
    while (1)
    {
        uint32_t tmp_tx_send_index = tx_send_index;
        /* send, or try to send, data */
        /* prepare send buffer */
        for (i = 0; i < 16; i++)
        {
            tmp_tx_buf[i] = g_uart_2_tx_data[tmp_tx_send_index++];
            if (tmp_tx_send_index == 32) tmp_tx_send_index = 0;
        }
        /* send date */
        words_written = etpu_uart_transmit_data(&uart_2_instance, &uart_2_config, tmp_tx_buf, 16);
        tx_send_index = (tx_send_index + words_written) % 32;
        
        /* peridoically do a read */
        current_time = read_time();
        if (current_time - previous_time > 1000.0)
        {
            int32_t fifo_used;
            
            previous_time = current_time;
            etpu_uart_receive_fifo_status(&uart_2_instance, &uart_2_config, 0, &fifo_used);
            words_read = etpu_uart_receive_data(&uart_2_instance, &uart_2_config, g_uart_2_rx_data, 5, &overrun_error_status);
            for (i = 0; i < words_read; i++)
            {
                /* verify data */
                if (g_uart_2_rx_data[i].rx_data_word != g_uart_2_tx_data[rx_read_index++])
                {
                }
                if (rx_read_index == 32) rx_read_index = 0;
            }
            if (++j == 10) break;
        }
    }


	/* TESTING DONE */
	
	g_complete_flag = 1;

	while (1)
		;

	return 0;
}
