#include <zephyr/kernel.h>
#include "analyzer_config.h"

// Hardware headers for ESP32 I2S, DMA, and DPORT Clocks
#include <soc/i2s_struct.h>
#include <soc/i2s_reg.h>
#include <rom/lldesc.h>
#include <soc/soc.h>
#include <soc/dport_reg.h>

// RAM block where data is stored
la_sample_t capture_ram[RING_BUFFER_SAMPLES];

// ESP32 DMA descriptor size field is limited to 4095 bytes max per node.
#define DMA_CHUNK_SIZE 4000
#define NUM_DMA_NODES ((RING_BUFFER_SAMPLES * sizeof(la_sample_t)) / DMA_CHUNK_SIZE)

__attribute__((aligned(4))) lldesc_t dma_nodes[NUM_DMA_NODES];

/*
 APB clock is 80MHz on classic ESP32. I2S RX sample clock in camera mode is:
 sample_clk = APB_CLK / clkm_div_num / rx_bck_div_num
 With clkm_div_num=40 and rx_bck_div_num=1 this gives 80MHz/40 = 2MHz.
 Tune clkm_div_num for your target sample rate (higher divider = slower rate).
 clkm_div_a/clkm_div_b are the fractional-divide numerator/denominator; 0/0
 disables the fractional part and uses an integer divide by clkm_div_num.
 */
#define I2S_CLKM_DIV_NUM 40
#define I2S_BCK_DIV_NUM  1

void init_dma_ring_buffer(void) {
    // Build an infinite circular linked list of 4000-byte blocks
    for (int i = 0; i < NUM_DMA_NODES; i++) {
        dma_nodes[i].buf = (uint8_t *)&capture_ram[i * (DMA_CHUNK_SIZE / sizeof(la_sample_t))];
        dma_nodes[i].size = DMA_CHUNK_SIZE;
        dma_nodes[i].length = DMA_CHUNK_SIZE;
        dma_nodes[i].owner = 1; // Owned by DMA hardware
        // Set EOF on every node so in_eof_des_addr updates once per chunk instead of once per full ring lap. This makes get_current_dma_write_index() track live progress instead of jumping once per NUM_DMA_NODES chunks.
        dma_nodes[i].eof = 1;

        // Link to the next node, looping back to 0 at the end
        dma_nodes[i].qe.stqe_next = &dma_nodes[(i + 1) % NUM_DMA_NODES];
    }


    // Enable I2S0 clock using classic DPORT register masks
    SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_I2S0_CLK_EN);
    CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_I2S0_RST);

    // Reset I2S0 RX & FIFO
    I2S0.conf.rx_reset = 1;
    I2S0.conf.rx_reset = 0;
    I2S0.conf.rx_fifo_reset = 1;
    I2S0.conf.rx_fifo_reset = 0;
    I2S0.lc_conf.in_rst = 1;
    I2S0.lc_conf.in_rst = 0;

    // Configure Camera Internal Master Mode for Testing
    I2S0.conf.rx_slave_mod = 0;      // Set as MASTER (Internal clock generation)
    I2S0.conf2.camera_en = 1;        // Enable Camera RX mode
    I2S0.sample_rate_conf.rx_bits_mod = 8;
    I2S0.fifo_conf.rx_data_num = 32;
    I2S0.fifo_conf.dscr_en = 1;      // Enable DMA descriptors

    I2S0.clkm_conf.clka_en = 0;              // use APB clock
    I2S0.clkm_conf.clkm_div_num = I2S_CLKM_DIV_NUM;
    I2S0.clkm_conf.clkm_div_a = 0;           
    I2S0.clkm_conf.clkm_div_b = 0;
    I2S0.sample_rate_conf.rx_bck_div_num = I2S_BCK_DIV_NUM;

    // Load custom linked list descriptor address into silicon
    I2S0.in_link.addr = (uint32_t)&dma_nodes[0];
    I2S0.in_link.start = 1;

    // Start RX DMA
    I2S0.conf.rx_start = 1;

    printk("ESP32 I2S Multi-Node DMA Initialized & Active.\n");
    printk("[I2S] sample_clk ~= %u Hz\n", (unsigned)(80000000UL / I2S_CLKM_DIV_NUM / I2S_BCK_DIV_NUM));
}

uint32_t get_current_dma_write_index(void) {

    lldesc_t *eof_desc = (lldesc_t *)I2S0.in_eof_des_addr;

    if (eof_desc == NULL) {
        return 0;
    }

    uint32_t offset_bytes = (uint32_t)eof_desc->buf - (uint32_t)&capture_ram[0];
    return offset_bytes / sizeof(la_sample_t);
}