#define one_over_sqrt_2 0.70710678118d

#define state_machine 0  // The PIO state machine to use
#define width 256        // Bitmap width in pixels
#define height 256       // Bitmap height in pixels
#define hsync_bp1 24     // Length of pulse at 0.0v
#define hsync_bp2 48     // Length of pulse at 0.3v
#define hdots 382        // Data for hsync including back porch
#define piofreq 7.0f     // Clock frequence of state machine
#define border_colour 11 // The border colour

#define pixel_start hsync_bp1 + hsync_bp2 + 18 // Where the pixel data starts in pixel_buffer

#define DATA_PIN 14
#define CLOCK_PIN 15

unsigned int keycode_buffer = 0;
bool a_pressed = false, d_pressed = false, w_pressed = false;

uint dma_channel; // DMA channel for transferring hsync data to PIO
uint vline;       // Current video line being processed
uint bline;       // Line in the screen_buffer to fetch

unsigned char screen_buffer_out[height][width];
unsigned char screen_buffer_in[height][width];

unsigned char vsync_ll[hdots + 1];        // buffer for a vsync line with a long/long pulse
unsigned char vsync_ls[hdots + 1];        // buffer for a vsync line with a long/short pulse
unsigned char vsync_ss[hdots + 1];        // Buffer for a vsync line with a short/short pulse
unsigned char border[hdots + 1];          // Buffer for a vsync line for the top and bottom borders
unsigned char pixel_buffer[2][hdots + 1]; // Double-buffer for the pixel data scanlines

unsigned int frameCounter = 0;

unsigned char stroke = 0x1f;
unsigned char fill = 0x1f;

volatile bool done_frame = false;

//----------------------------------------------------drawing funcions---------------------------------------------

void background(unsigned char color)
{
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
        {
            screen_buffer_in[i][j] = color;
        }
}

void line(int x0, int y0, int x1, int y1)
{

    int dx = abs(x1 - x0);
    int8_t sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int8_t sy = y0 < y1 ? 1 : -1;
    int err = dx + dy; /* error value e_xy */
    while (true)
    { /* loop */
        if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height)
            screen_buffer_in[y0][x0] = stroke;
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            /* e_xy+e_x > 0 */
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            /* e_xy+e_y < 0 */
            err += dx;
            y0 += sy;
        }
    }
}

void box(int x, int y, int w, int h)
{
    if (w < 0)
    {
        x += w;
        w = -w;
    }
    if (h < 0)
    {
        y += h;
        h = -h;
    }

    for (int i = x; i < x + w; i++)
        for (int j = y; j < y + h; j++)
            if (i >= 0 && i < width && j >= 0 && j < height)
                screen_buffer_in[j][i] = fill;
}

void circle(int x, int y, uint r)
{
    uint r2 = r * r;
    uint r45 = floor(r * one_over_sqrt_2);
    box(x - r45, y - r45, 2 * r45, 2 * r45);
    for (int i = x - r45 - 1, imax = x + r45 + 1; i < imax; i++)
    {
        int i_absolute = i - x;
        int i2 = i_absolute * i_absolute;
        for (int j = y - r - 1, jmax = y - r45 + 1; j < jmax; j++)
        {
            int j_absolute = j - y;
            if (i2 + j_absolute * j_absolute <= r2)
            {
                if (i >= 0 && i < width && j >= 0 && j < height)
                    screen_buffer_in[j][i] = fill;
            }
        }
        for (int j = y + r45 - 1, jmax = y + r + 1; j < jmax; j++)
        {
            int j_absolute = j - y;
            if (i2 + j_absolute * j_absolute <= r2)
            {
                if (i >= 0 && i < width && j >= 0 && j < height)
                    screen_buffer_in[j][i] = fill;
            }
        }
    }
    for (int j = y - r45 - 1, jmax = y + r45 + 1; j < jmax; j++)
    {
        int j_absolute = j - y;
        int j2 = j_absolute * j_absolute;
        for (int i = x - r - 1, imax = x - r45 + 1; i < imax; i++)
        {
            int i_absolute = i - x;
            if (j2 + i_absolute * i_absolute <= r2)
            {
                if (i >= 0 && i < width && j >= 0 && j < height)
                    screen_buffer_in[j][i] = fill;
            }
        }
        for (int i = x + r45 - 1, imax = x + r + 1; i < imax; i++)
        {
            int i_absolute = i - y;
            if (j2 + i_absolute * i_absolute <= r2)
            {
                if (i >= 0 && i < width && j >= 0 && j < height)
                    screen_buffer_in[j][i] = fill;
            }
        }
    }
}

void text(char *text, int x, int y)
{
    for (int i = 0; i < strlen(text); i++)
    {
        char letter = text[i];
        for (int j = 0; j < 13; j++)
        {
            unsigned char line = font[letter][j];
            for (int k = 0; k < 8; k++)
            {
                bool bit = (line >> (7 - k)) & 1;
                if (bit)
                {
                    int yb = y - j;
                    int xb = x + (i * 10) + k;
                    if (xb >= 0 && xb < width && yb >= 0 && yb < height)
                        screen_buffer_in[yb][xb] = fill;
                }
            }
        }
    }
}

void get_code()
{
    int got = pio_sm_get_blocking(pio1, 0);
    int data = (got >> 22) & 0xff; //right justify
    // bool start_bit = (data >> 1) & 0x1;
    // data = (data >> 2) & 0x3ff; //shift out filler and start bit
    // bool parity_bit = (data >> 8) & 0x1;
    // bool stop_bit = (data >> 9) & 0x1;
    // data &= 0xff; //mask out parity and stop bit
    // bool calc_parity = !((0x6996u >> ((data ^ (data >> 4)) & 0xf)) & 1);
    // printf("data: 0x%x\n", data);

    keycode_buffer = keycode_buffer << 8;
    keycode_buffer &= 0xffffff00;
    keycode_buffer += data;
    // printf("keycode_buffer: 0x%x\n", keycode_buffer);
    bool not_break_code = !((keycode_buffer & 0x0000ff00) == 0xf000);
    // printf("breakcode: %i\n", not_break_code);
    switch (data)
    {
    case 0x1c: //a
        a_pressed = not_break_code;
        break;
    case 0x23: //d
        d_pressed = not_break_code;
        break;
        break;
    case 0x1d: //w
        w_pressed = not_break_code;
        break;

    default:
        break;
    }
}

void keyboard_init()
{
    PIO pio = pio1;
    uint sm = 0;
    // Set up the state machine we're going to use to receive them.
    uint offset = pio_add_program(pio, &keyboard_pio_program);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, DATA_PIN);
    sm_config_set_in_shift(&c, true, true, 11);

    pio_sm_set_consecutive_pindirs(pio, sm, DATA_PIN, 2, false);
    pio_set_irq0_source_enabled(pio, pis_sm0_rx_fifo_not_empty, true);
    // pio_set_irq0_source_enabled(pio, pis_interrupt0, false);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    irq_set_exclusive_handler(PIO1_IRQ_0, &get_code);
    irq_set_enabled(PIO1_IRQ_0, true);
}

void av_init()
{
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &cvideo_program); // Load up the PIO program

    dma_channel = dma_claim_unused_channel(true); // Claim a DMA channel for the hsync transfer
    vline = 1;                                    // Initialise the video scan line counter to 1
    bline = 0;                                    // And the index into the screen_buffer pixel buffer to 0

    write_vsync_l(&vsync_ll[0], hdots >> 1); // Pre-build a long/long vscan line...
    write_vsync_l(&vsync_ll[hdots >> 1], hdots >> 1);
    write_vsync_l(&vsync_ls[0], hdots >> 1); // A long/short vscan line...
    write_vsync_s(&vsync_ls[hdots >> 1], hdots >> 1);
    write_vsync_s(&vsync_ss[0], hdots >> 1); // A short/short vscan line
    write_vsync_s(&vsync_ss[hdots >> 1], hdots >> 1);

    // This bit pre-builds the border scanline
    //
    memset(&border[0], border_colour, hdots); // Fill the border with the border colour
    memset(&border[0], 1, hsync_bp1);         // Add the hsync pulse
    memset(&border[hsync_bp1], 9, hsync_bp2);

    // This bit pre-builds the pixel buffer scanlines by adding the hsync pulse and left and right horizontal borders
    //
    for (int i = 0; i < 2; i++)
    {                                                      // Loop through the pixel buffers
        memset(&pixel_buffer[i][0], border_colour, hdots); // First fill the buffer with the border colour
        memset(&pixel_buffer[i][0], 1, hsync_bp1);         // Add the hsync pulse
        memset(&pixel_buffer[i][hsync_bp1], 9, hsync_bp2);
        memset(&pixel_buffer[i][pixel_start], 31, width);
    }

    // Initialise the PIO
    //
    pio_sm_set_enabled(pio, state_machine, false);                        // Disable the PIO state machine
    pio_sm_clear_fifos(pio, state_machine);                               // Clear the PIO FIFO buffers
    cvideo_initialise_pio(pio, state_machine, offset, 0, 5, piofreq);     // Initialise the PIO (function in cvideo.pio)
    cvideo_configure_pio_dma(pio, state_machine, dma_channel, hdots + 1); // Hook up the DMA channel to the state machine
    pio_sm_set_enabled(pio, state_machine, true);                         // Enable the PIO state machine

    background(0x0b);                //in buffer
    for (int i = 0; i < height; i++) //out buffer
        for (int j = 0; j < width; j++)
        {
            screen_buffer_out[i][j] = 0x1f;
        }

    // And kick everything off

    cvideo_dma_handler(); // Call the DMA handler as a one-off to initialise it
}

// Write out a short vsync pulse
// Parameters:
// - p: Pointer to the buffer to store this sync data
// - length: The buffer size
//
void write_vsync_s(unsigned char *p, int length)
{
    int pulse_width = length / 16;
    for (int i = 0; i < length; i++)
    {
        p[i] = i <= pulse_width ? 1 : 13;
    }
}

// Write out a long vsync half-pulse
// Parameters:
// - p: Pointer to the buffer to store this sync data
// - length: The buffer size
//
void write_vsync_l(unsigned char *p, int length)
{
    int pulse_width = length - (length / 16) - 1;
    for (int i = 0; i < length; i++)
    {
        p[i] = i >= pulse_width ? 13 : 1;
    }
}

// The hblank interrupt handler
// This is triggered by the instruction irq set 0 in the PIO code (cvideo.pio)
//
void cvideo_dma_handler(void)
{

    // Switch condition on the vertical scanline number (vline)
    // Each statement does a dma_channel_set_read_addr to point the PIO to the next data to output
    //
    switch (vline)
    {

    // First deal with the vertical sync scanlines
    // Also on scanline 3, preload the first pixel buffer scanline
    //
    case 1 ... 2:
        dma_channel_set_read_addr(dma_channel, vsync_ll, true);
        break;
    case 3:
        dma_channel_set_read_addr(dma_channel, vsync_ls, true);
        memcpy(&pixel_buffer[bline & 1][pixel_start], &screen_buffer_out[bline], width);
        break;
    case 4 ... 5:
    case 310 ... 312:
        dma_channel_set_read_addr(dma_channel, vsync_ss, true);
        break;

    // Then the border scanlines
    case 6 ... 38:
    case 294 ... 309:
        dma_channel_set_read_addr(dma_channel, border, true);
        break;

    // Now point the dma at the first buffer for the pixel data,
    // and preload the data for the next scanline
    //
    default:
        dma_channel_set_read_addr(dma_channel, pixel_buffer[bline++ & 1], true);         // Set the DMA to read from one of the pixel_buffers
        memcpy(&pixel_buffer[bline & 1][pixel_start], &screen_buffer_out[bline], width); // And memcpy the next scanline into the other pixel buffer
        break;
    }

    // Increment and wrap the counters
    //
    if (vline++ >= 312)
    {              // If we've gone past the bottom scanline then
        vline = 1; // Reset the scanline counter
        bline = 0; // And the pixel buffer row index counter
        done_frame = true;
    }

    // Finally, clear the interrupt request ready for the next horizontal sync interrupt
    //
    dma_hw->ints0 = 1u << dma_channel;
}

// Configure the PIO DMA
// Parameters:
// - pio: The PIO to attach this to
// - sm: The state machine number
// - dma_channel: The DMA channel
// - buffer_size_words: Number of bytes to transfer
//
void cvideo_configure_pio_dma(PIO pio, uint sm, uint dma_channel, size_t buffer_size_words)
{
    pio_sm_clear_fifos(pio, sm);
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    dma_channel_configure(dma_channel, &c,
                          &pio->txf[sm],     // Destination pointer
                          NULL,              // Source pointer
                          buffer_size_words, // Number of transfers
                          true               // Start flag (true = start immediately)
    );
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, cvideo_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}
