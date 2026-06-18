module MuraxDe10Standard (
    input  wire       CLOCK_50,
    input  wire       KEY0,
    output wire [9:0] LEDR
);
    wire [31:0] gpio_write;
    wire [31:0] gpio_write_enable;
    wire        unused_uart_txd;

    Murax murax (
        .io_asyncReset       (~KEY0),
        .io_mainClk          (CLOCK_50),
        .io_gpioA_read       (32'b0),
        .io_gpioA_write      (gpio_write),
        .io_gpioA_writeEnable(gpio_write_enable),
        .io_uart_txd         (unused_uart_txd),
        .io_uart_rxd         (1'b1)
    );

    assign LEDR[3:0] = gpio_write[3:0] & gpio_write_enable[3:0];
    assign LEDR[9:4] = 6'b0;
endmodule
