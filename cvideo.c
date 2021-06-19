#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "cvideo.h"
#include "cvideo.pio.h" // The assembled PIO code
#include "font.h"
#include "av.h"
#include "map.h"

//TODO: map as a grid, raycasting

#define USB_BOOT 17 //pin for usb boot

int pos_x = -128, pos_y = -128;
volatile float angle = 2;

char char_buffer[32];
void buttons()
{
    /* if (gpio_get(LEFT_BUTTON))
    {
        angle = angle - 0.1;
    }
    if (gpio_get(RIGHT_BUTTON))
    {
        angle = angle + 0.1;
    }
    if (gpio_get(FORWOARD_BUTTON))
    {
        pos_x += cos(angle) * 5;
        pos_y += sin(angle) * 5;
    }*/
    if (a_pressed)
    {
        angle = angle - 0.1;
    }
    if (d_pressed)
    {
        angle = angle + 0.1;
    }
    if (w_pressed)
    {
        pos_x += cos(angle) * 5;
        pos_y += sin(angle) * 5;
    }
}

void draw_map()
{
    for (int i = 0; i < MAP_SIZE; i++)
        line(map[i].x1 - pos_x, map[i].y1 - pos_y, map[i].x2 - pos_x, map[i].y2 - pos_y);
}

void draw()
{
    // sprintf(char_buffer, "%f", angle);
    // sprintf(char_buffer, "0x%x", keycode_buffer);
    background(0x10);
    draw_map();
    line(128, 128, 128 + cos(angle) * 10, 128 + sin(angle) * 10);

    // text(char_buffer, 10, 20);

    frameCounter++;
    buttons();
}

void main_core1()
{

    keyboard_init();
    multicore_fifo_push_blocking(0xffffffff); //prelaoad fifo
    while (true)
    {
        multicore_fifo_push_blocking(0xffffffff); //wait for next done frame
        draw();                                   //start drawing
    }
}

int main()
{

    stdio_init_all();
    gpio_set_dir(USB_BOOT, GPIO_IN);
    gpio_pull_down(USB_BOOT);

    if (gpio_get(USB_BOOT))
    {
        reset_usb_boot(0, 0);
    }
    /*
    gpio_set_dir(LEFT_BUTTON, GPIO_IN);
    gpio_pull_down(LEFT_BUTTON);

    gpio_set_dir(RIGHT_BUTTON, GPIO_IN);
    gpio_pull_down(RIGHT_BUTTON);

    gpio_set_dir(FORWOARD_BUTTON, GPIO_IN);
    gpio_pull_down(FORWOARD_BUTTON);
*/
    multicore_launch_core1(main_core1);

    av_init();

    // text("no to jedziemy", 50, 50);

    while (true)
    {
        if (gpio_get(USB_BOOT))
        {
            reset_usb_boot(0, 0);
        }
        if (done_frame) //when frame is done drawnig
        {
            done_frame = false;
            if (multicore_fifo_rvalid) //and there is a new frame in in buffer
            {
                memcpy(screen_buffer_out, screen_buffer_in, width * height); //copy it to out buffer
                multicore_fifo_pop_blocking();                               //start drawing the next frame
            }
        }
    }
}
