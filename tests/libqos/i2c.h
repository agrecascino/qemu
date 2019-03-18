/*
 * I2C libqos
 *
 * Copyright (c) 2012 Andreas Färber
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef LIBQOS_I2C_H
#define LIBQOS_I2C_H

#include "libqtest.h"

typedef struct I2CAdapter I2CAdapter;
struct I2CAdapter {
    void (*send)(I2CAdapter *adapter, uint8_t addr,
                 const uint8_t *buf, uint16_t len);
    void (*recv)(I2CAdapter *adapter, uint8_t addr,
                 uint8_t *buf, uint16_t len);

    QTestState *qts;
};

#define OMAP2_I2C_1_BASE 0x48070000

void i2c_send(I2CAdapter *i2c, uint8_t addr,
              const uint8_t *buf, uint16_t len);
void i2c_recv(I2CAdapter *i2c, uint8_t addr,
              uint8_t *buf, uint16_t len);

void i2c_read_block(I2CAdapter *i2c, uint8_t addr, uint8_t reg,
                    uint8_t *buf, uint16_t len);
void i2c_write_block(I2CAdapter *i2c, uint8_t addr, uint8_t reg,
                     const uint8_t *buf, uint16_t len);
uint8_t i2c_get8(I2CAdapter *i2c, uint8_t addr, uint8_t reg);
uint16_t i2c_get16(I2CAdapter *i2c, uint8_t addr, uint8_t reg);
void i2c_set8(I2CAdapter *i2c, uint8_t addr, uint8_t reg,
              uint8_t value);
void i2c_set16(I2CAdapter *i2c, uint8_t addr, uint8_t reg,
               uint16_t value);

/* libi2c-omap.c */
I2CAdapter *omap_i2c_create(QTestState *qts, uint64_t addr);

/* libi2c-imx.c */
I2CAdapter *imx_i2c_create(QTestState *qts, uint64_t addr);

#endif
