/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include "syscfg/syscfg.h"
#include <flash_map/flash_map.h>
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_bsp.h>
#include <hal/hal_system.h>
#include <hal/hal_flash.h>
#include <os/os_cputime.h>

#if MYNEWT_VAL(BOOT_SERIAL)
#include <hal/hal_gpio.h>
#include <boot_serial/boot_serial.h>
#include <sysinit/sysinit.h>
#endif
#include <console/console.h>
#include "bootutil/image.h"
#include "bootutil/bootutil.h"

#define BOOT_AREA_DESC_MAX  (256)
#define AREA_DESC_MAX       (BOOT_AREA_DESC_MAX)

#if MYNEWT_VAL(BOOT_SERIAL)
#define BOOT_SER_CONS_INPUT         128
#endif

/* Minimal interval in ms that DFU pin must be pressed to go into DFU mode */
#define BOOTLOADER_BUTTON_HOLDING_INTERVAL 500

#define BLINKY_PERIOD               125000

struct hal_timer _blinky_timer;

void 
blinky_isr(void *arg)
{
  hal_gpio_toggle(LED_BLINK_PIN);

  os_cputime_timer_relative(&_blinky_timer, BLINKY_PERIOD);
}

void
start_boot_serial_mode(void)
{
  /* Set up fast blinky for indicator using hw timer */
  os_cputime_timer_init(&_blinky_timer, blinky_isr, NULL);
  os_cputime_timer_relative(&_blinky_timer, BLINKY_PERIOD);

  boot_serial_start(BOOT_SER_CONS_INPUT);
  assert(0);
}

int
main(void)
{
    struct boot_rsp rsp;
    int rc;

#if MYNEWT_VAL(BOOT_SERIAL)
    sysinit();
#else
    flash_map_init();
    hal_bsp_init();
#endif

#if MYNEWT_VAL(BOOT_SERIAL)
    /* Check if Magic number is REST_TO_DFU */
    if (BOOTLOADER_MAGIC_LOC ==  BOOTLOADER_RESET_TO_DFU_MAGIC)
    {
        start_boot_serial_mode();
    }

    /*
     * Configure a GPIO as input, and compare it against expected value.
     * If it matches, await for download commands from serial.
     */
    hal_gpio_init_in(BOOT_SERIAL_DETECT_PIN, BOOT_SERIAL_DETECT_PIN_CFG);
    if (hal_gpio_read(BOOT_SERIAL_DETECT_PIN) == BOOT_SERIAL_DETECT_PIN_VAL) {

        /* Double check the pin value after configured interval,
         * make sure the DFU pin hold long enough */
         os_cputime_delay_usecs( 1000*BOOTLOADER_BUTTON_HOLDING_INTERVAL );

        if (hal_gpio_read(BOOT_SERIAL_DETECT_PIN) == BOOT_SERIAL_DETECT_PIN_VAL) {
            start_boot_serial_mode();
        }
    }
#endif

    /* Go on with normal boot progress */
    rc = boot_go(&rsp);

#if 0
    /* No bootable image, go to boot_serial dfu mode
     * Does not work due to assert() somewhere in boot_go() */
    if ( rc )
    {
        start_boot_serial_mode();
        assert(0);
    }
#endif

    assert(rc == 0);

    hal_system_start((void *)(rsp.br_image_addr + rsp.br_hdr->ih_hdr_size));

    return 0;
}
