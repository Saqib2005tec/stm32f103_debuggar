#include "swd.h"

#include <stdio.h>
#include "swd_log.h"
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"


static spi_device_handle_t swd_spi_handle;

static swd_status_t swd_last_status = SWD_OK;


swd_status_t swd_get_last_status(void)
{
    return swd_last_status;
}



const char *swd_status_string(
    swd_status_t status
)
{
    switch (status)
    {
        case SWD_OK:
            return "OK";

        case SWD_ERR_INVALID_ARG:
            return "INVALID_ARGUMENT";

        case SWD_ERR_TIMEOUT:
            return "TIMEOUT";

        case SWD_ERR_ACK_WAIT:
            return "ACK_WAIT";

        case SWD_ERR_ACK_FAULT:
            return "ACK_FAULT";

        case SWD_ERR_ACK_INVALID:
            return "ACK_INVALID";

        case SWD_ERR_PARITY:
            return "PARITY_ERROR";

        case SWD_ERR_DP_STICKY_ERROR:
            return "DP_STICKY_ERROR";

        case SWD_ERR_AP_FAULT:
            return "AP_FAULT";

        case SWD_ERR_TARGET_NOT_HALTED:
            return "TARGET_NOT_HALTED";

        case SWD_ERR_REGISTER_NOT_READY:
            return "REGISTER_NOT_READY";

        default:
            return "UNKNOWN";
    }
}

static uint8_t swd_parity32(
    uint32_t value
)
{
    value ^= value >> 16;
    value ^= value >> 8;
    value ^= value >> 4;
    value ^= value >> 2;
    value ^= value >> 1;

    return value & 1U;
}

/*
 * ---------------------------------------------------------
 * Calculate parity of a 4-bit value.
 * ---------------------------------------------------------
 *
 * SWD request parity covers:
 *
 * APnDP
 * RnW
 * A2
 * A3
 */
static uint8_t swd_request_parity(
    bool ap,
    bool read,
    uint8_t addr
)
{
    uint8_t a2 = (addr >> 2) & 0x01;
    uint8_t a3 = (addr >> 3) & 0x01;

    return ap ^ read ^ a2 ^ a3;
}


/*
 * ---------------------------------------------------------
 * Construct SWD request header
 * ---------------------------------------------------------
 *
 * Bit 0 = Start
 * Bit 1 = APnDP
 * Bit 2 = RnW
 * Bit 3 = A2
 * Bit 4 = A3
 * Bit 5 = Parity
 * Bit 6 = Stop
 * Bit 7 = Park
 */
uint8_t swd_make_request(
    bool ap,
    bool read,
    uint8_t addr
)
{
    uint8_t a2 = (addr >> 2) & 0x01;
    uint8_t a3 = (addr >> 3) & 0x01;

    uint8_t parity =
        swd_request_parity(ap, read, addr);

    uint8_t request = 0;

    /*
     * Start
     */
    request |= (1U << 0);

    /*
     * APnDP
     */
    request |= ((uint8_t)ap << 1);

    /*
     * RnW
     */
    request |= ((uint8_t)read << 2);

    /*
     * A2
     */
    request |= (a2 << 3);

    /*
     * A3
     */
    request |= (a3 << 4);

    /*
     * Parity
     */
    request |= (parity << 5);

    /*
     * Stop = 0
     *
     * No need to explicitly OR it.
     */

    /*
     * Park = 1
     */
    request |= (1U << 7);

    return request;
}


/*
 * ---------------------------------------------------------
 * Initialize NRST
 * ---------------------------------------------------------
 */


/*
 * ---------------------------------------------------------
 * Initialize SPI peripheral
 * ---------------------------------------------------------
 */
void swd_init(void)
{
    // swd_nrst_init(); //reset the stm32

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = SWDIO_GPIO,
        .sclk_io_num = SWCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };

    ESP_ERROR_CHECK(
        spi_bus_initialize(
            SPI2_HOST,
            &buscfg,
            SPI_DMA_DISABLED
        )
    );

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 7,

        .flags =
            SPI_DEVICE_HALFDUPLEX |
            SPI_DEVICE_3WIRE |
            SPI_DEVICE_TXBIT_LSBFIRST |
            SPI_DEVICE_RXBIT_LSBFIRST
    };


    gpio_config_t nrst_config = {
    .pin_bit_mask = (1ULL << NRST_GPIO),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
};

gpio_config(&nrst_config);

gpio_set_level(NRST_GPIO, 1);

    ESP_ERROR_CHECK(
        spi_bus_add_device(
            SPI2_HOST,
            &devcfg,
            &swd_spi_handle
        )
    );

    SWD_LOG("SWD SPI transport initialized.\n");
}


/*
 * ---------------------------------------------------------
 * JTAG -> SWD
 * ---------------------------------------------------------
 */
void swd_jtag_to_swd(void)
{
    uint8_t init_seq[] = {
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,

        0x9E, 0xE7,

        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,

        0x00
    };

    spi_transaction_t t = {
        .length = sizeof(init_seq) * 8,
        .tx_buffer = init_seq
    };

    ESP_ERROR_CHECK(
        spi_device_polling_transmit(
            swd_spi_handle,
            &t
        )
    );

    SWD_LOG("JTAG -> SWD sequence sent.\n");
}


/*
 * ---------------------------------------------------------
 * Generic SWD WRITE
 * ---------------------------------------------------------
 */
bool swd_write(
    bool ap,
    uint8_t addr,
    uint32_t data
)
{
    uint8_t request =
        swd_make_request(
            ap,
            false,
            addr
        );

    for (int retry = 0;
         retry < SWD_MAX_RETRIES;
         retry++)
    {
        uint8_t rx_ack = 0;

        spi_transaction_t t_header = {
            .length = 8,
            .rxlength = 5,
            .tx_buffer = &request,
            .rx_buffer = &rx_ack
        };

        ESP_ERROR_CHECK(
            spi_device_polling_transmit(
                swd_spi_handle,
                &t_header
            )
        );

        uint8_t ack =
            (rx_ack >> 1) & 0x07;


        /*
         * ---------------------------------------------
         * ACK = WAIT
         * ---------------------------------------------
         */

        if (ack == SWD_ACK_WAIT)
        {
            swd_last_status =
                SWD_ERR_ACK_WAIT;

            continue;
        }


        /*
         * ---------------------------------------------
         * ACK = FAULT
         * ---------------------------------------------
         */

        if (ack == SWD_ACK_FAULT)
        {
            swd_last_status =
                SWD_ERR_ACK_FAULT;

            SWD_LOG(
                "SWD WRITE FAULT: "
                "AP=%d ADDR=0x%02X\n",
                ap,
                addr
            );

            return false;
        }


        /*
         * ---------------------------------------------
         * Invalid ACK
         * ---------------------------------------------
         */

        if (ack != SWD_ACK_OK)
        {
            swd_last_status =
                SWD_ERR_ACK_INVALID;

            SWD_LOG(
                "SWD WRITE invalid ACK: "
                "0x%02X\n",
                ack
            );

            return false;
        }


        /*
         * ---------------------------------------------
         * ACK = OK
         *
         * Now send data.
         * ---------------------------------------------
         */

        uint8_t parity =
            swd_parity32(data);

        uint64_t tx_data =
            (uint64_t)data |
            ((uint64_t)parity << 32);


        spi_transaction_t t_data = {
            .length = 41,
            .tx_buffer = &tx_data
        };

        ESP_ERROR_CHECK(
            spi_device_polling_transmit(
                swd_spi_handle,
                &t_data
            )
        );

        swd_last_status = SWD_OK;

        return true;
    }


    /*
     * Retry limit reached.
     */

    swd_last_status =
        SWD_ERR_TIMEOUT;

    SWD_LOG(
        "SWD WRITE timeout: "
        "AP=%d ADDR=0x%02X\n",
        ap,
        addr
    );

    return false;
}


/*
 * ---------------------------------------------------------
 * Generic SWD READ
 * ---------------------------------------------------------
 */
uint32_t swd_read(
    bool ap,
    uint8_t addr,
    uint8_t *ack_out
)
{
    uint8_t request =
        swd_make_request(
            ap,
            true,
            addr
        );

    for (int retry = 0;
         retry < SWD_MAX_RETRIES;
         retry++)
    {
        uint8_t rx_data[5] = {0};

        /*
         * -------------------------------------------------
         * Send request and receive:
         *
         * 1 turnaround
         * 3 ACK
         * 32 DATA
         * 1 parity
         *
         * Total = 37 bits
         * -------------------------------------------------
         */

        spi_transaction_t t = {
            .length = 8,
            .rxlength = 37,
            .tx_buffer = &request,
            .rx_buffer = rx_data
        };

        ESP_ERROR_CHECK(
            spi_device_polling_transmit(
                swd_spi_handle,
                &t
            )
        );


        /*
         * -------------------------------------------------
         * Extract ACK
         * -------------------------------------------------
         */

        uint8_t ack =
            (rx_data[0] >> 1) & 0x07;

        if (ack_out != NULL)
        {
            *ack_out = ack;
        }


        /*
         * -------------------------------------------------
         * ACK = WAIT
         * -------------------------------------------------
         */

        if (ack == SWD_ACK_WAIT)
        {
            swd_last_status =
                SWD_ERR_ACK_WAIT;

            continue;
        }


        /*
         * -------------------------------------------------
         * ACK = FAULT
         * -------------------------------------------------
         */

        if (ack == SWD_ACK_FAULT)
        {
            swd_last_status =
                SWD_ERR_ACK_FAULT;

            SWD_LOG(
                "SWD READ FAULT: "
                "AP=%d ADDR=0x%02X\n",
                ap,
                addr
            );

            return 0;
        }


        /*
         * -------------------------------------------------
         * Invalid ACK
         * -------------------------------------------------
         */

        if (ack != SWD_ACK_OK)
        {
            swd_last_status =
                SWD_ERR_ACK_INVALID;

            SWD_LOG(
                "SWD READ invalid ACK: "
                "0x%02X\n",
                ack
            );

            return 0;
        }


        /*
         * -------------------------------------------------
         * Extract 32-bit data
         * -------------------------------------------------
         */

        uint32_t data = 0;

        data |=
            ((uint32_t)rx_data[0] >> 4);

        data |=
            ((uint32_t)rx_data[1] << 4);

        data |=
            ((uint32_t)rx_data[2] << 12);

        data |=
            ((uint32_t)rx_data[3] << 20);

        data |=
            ((uint32_t)rx_data[4] << 28);


        /*
         * -------------------------------------------------
         * Extract received parity
         *
         * Bit 36 of the SWD response is the parity bit.
         * -------------------------------------------------
         */

        uint8_t received_parity =
            (rx_data[4] >> 4) & 0x01;


        /*
         * -------------------------------------------------
         * Calculate parity locally
         * -------------------------------------------------
         */

        uint8_t calculated_parity =
            swd_parity32(data);


        /*
         * -------------------------------------------------
         * Verify parity
         * -------------------------------------------------
         */

        if (received_parity != calculated_parity)
        {
            swd_last_status =
                SWD_ERR_PARITY;

            SWD_LOG(
                "SWD READ parity error: "
                "DATA=0x%08lX "
                "RX=%u "
                "CALC=%u\n",
                (unsigned long)data,
                received_parity,
                calculated_parity
            );

            return 0;
        }


        /*
         * -------------------------------------------------
         * Give SWD line idle clocks.
         * -------------------------------------------------
         */

        uint8_t idle = 0;

        spi_transaction_t t_idle = {
            .length = 8,
            .tx_buffer = &idle
        };

        ESP_ERROR_CHECK(
            spi_device_polling_transmit(
                swd_spi_handle,
                &t_idle
            )
        );


        /*
         * -------------------------------------------------
         * Success
         * -------------------------------------------------
         */

        swd_last_status = SWD_OK;

        return data;
    }


    /*
     * -----------------------------------------------------
     * Retry limit reached
     * -----------------------------------------------------
     */

    swd_last_status =
        SWD_ERR_TIMEOUT;

    SWD_LOG(
        "SWD READ timeout: "
        "AP=%d ADDR=0x%02X\n",
        ap,
        addr
    );

    return 0;
}


/*
 * ---------------------------------------------------------
 * Read DP IDCODE
 * ---------------------------------------------------------
 */
uint32_t swd_read_idcode(void)
{
    uint8_t ack;

    uint32_t idcode =
        swd_read(
            false,
            0x00,
            &ack
        );

    if (ack == SWD_ACK_OK) {
        SWD_LOG(
            "DP IDCODE = 0x%08lX\n",
            (unsigned long)idcode
        );
    }

    return idcode;
}

