/*
###############################################################################
#
# Copyright 2023, Gustavo Muro
# All rights reserved
#
# This file is part of EmbeddedFirmware.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#                                                                             */

/*==================[inclusions]=============================================*/
#include "sI2C.h"
#include "efHal.h"
#include "efHal_internal.h"

#if __has_include("sI2C_config.h")
    #include "sI2C_config.h"
#endif

/*==================[macros and typedef]=====================================*/

#ifndef sI2C_TOTAL
    #define sI2C_TOTAL     (1)
#endif

#ifndef sI2C_PERIOD
    #define sI2C_PERIOD              10      /* 10 microseconds -> 100000 khz */
#endif

#define SEMI_PERIOD         (sI2C_PERIOD / 2)

typedef struct
{
    efHal_gpio_id_t scl;
    efHal_gpio_id_t sda;
    sI2C_delay_t delay;
    efHal_dh_t efHal_dh_I2C;
}sI2C_data_t;

/*==================[internal functions declaration]=========================*/

/*==================[internal data definition]===============================*/

static sI2C_data_t sI2C_data[sI2C_TOTAL];

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

static void sendStart(sI2C_data_t *sI2C_data)
{
    efHal_gpio_setPin(sI2C_data->scl, 1);
    efHal_gpio_setPin(sI2C_data->sda, 1);

    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->sda, 0);
    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->scl, 0);
    sI2C_data->delay(SEMI_PERIOD);
}

static void sendStop(sI2C_data_t *sI2C_data)
{
    efHal_gpio_setPin(sI2C_data->scl, 0);
    efHal_gpio_setPin(sI2C_data->sda, 0);
    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->scl, 1);
    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->sda, 1);
}

static bool waitACK(sI2C_data_t *sI2C_data)
{
    bool ret = true;
    uint8_t timeout = 200;

    efHal_gpio_setPin(sI2C_data->sda, 1);
    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->scl, 1);

    while (timeout--)
    {
        sI2C_data->delay(SEMI_PERIOD);
        if (efHal_gpio_getPin(sI2C_data->sda) == 0)
        {
            ret = 0;
            break;
        }
    }

    efHal_gpio_setPin(sI2C_data->scl, 0);

    return ret;
}

static void sendAck(sI2C_data_t *sI2C_data)
{
    efHal_gpio_setPin(sI2C_data->scl, 0);
    efHal_gpio_setPin(sI2C_data->sda, 0);
    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->scl, 1);
    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->scl, 0);
}

static void sendNAck(sI2C_data_t *sI2C_data)
{
    efHal_gpio_setPin(sI2C_data->scl, 0);
    efHal_gpio_setPin(sI2C_data->sda, 1);
    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->scl, 1);
    sI2C_data->delay(SEMI_PERIOD);
    efHal_gpio_setPin(sI2C_data->scl, 0);
}

static void sendByte(sI2C_data_t *sI2C_data, uint8_t data)
{
    int i;

    efHal_gpio_setPin(sI2C_data->scl, 0);

    for (i = 0 ; i < 8 ; i++)
    {
        if (data & (0x80>>i))
            efHal_gpio_setPin(sI2C_data->sda, 1);
        else
            efHal_gpio_setPin(sI2C_data->sda, 0);

        sI2C_data->delay(SEMI_PERIOD);
        efHal_gpio_setPin(sI2C_data->scl, 1);
        sI2C_data->delay(SEMI_PERIOD);
        efHal_gpio_setPin(sI2C_data->scl, 0);
    }
}

static uint8_t readByte(sI2C_data_t *sI2C_data)
{
    int i;
    uint8_t ret = 0;

    efHal_gpio_setPin(sI2C_data->sda, 1);
    efHal_gpio_setPin(sI2C_data->scl, 0);

    for (i = 0 ; i < 8 ; i++)
    {
        sI2C_data->delay(SEMI_PERIOD);
        efHal_gpio_setPin(sI2C_data->scl, 1);
        sI2C_data->delay(SEMI_PERIOD);

        if (efHal_gpio_getPin(sI2C_data->sda))
            ret |= 0x80 >> i;

        efHal_gpio_setPin(sI2C_data->scl, 0);
    }

    return ret;
}

/*==================[external functions definition]==========================*/

extern void sI2C_init(void)
{
    int i;

    for (i = 0 ; i < sI2C_TOTAL ; i++)
    {
        sI2C_data[i].scl = EF_HAL_INVALID_ID;
        sI2C_data[i].sda = EF_HAL_INVALID_ID;
        sI2C_data[i].efHal_dh_I2C = NULL;
    }
}

extern sI2C_dh_t sI2C_open(efHal_gpio_id_t scl, efHal_gpio_id_t sda, sI2C_delay_t delay)
{
    void* ret = NULL;

    int i;

    for (i = 0 ; i < sI2C_TOTAL ; i++)
    {
        if (sI2C_data[i].scl == EF_HAL_INVALID_ID)
        {
            ret = &sI2C_data[i];

            sI2C_data[i].scl = scl;
            sI2C_data[i].sda = sda;
            sI2C_data[i].delay = delay;

            efHal_gpio_confPin(scl, EF_HAL_GPIO_OUTPUT_OD, EF_HAL_GPIO_PULL_DISABLE, 1);
            efHal_gpio_confPin(sda, EF_HAL_GPIO_OUTPUT_OD, EF_HAL_GPIO_PULL_DISABLE, 1);

            break;
        }
    }

    return ret;
}

extern void sI2C_set_efHal_dh(sI2C_dh_t dh, efHal_dh_t efHal_dh_I2C)
{
    sI2C_data_t *sI2C_data = dh;
    sI2C_data->efHal_dh_I2C = efHal_dh_I2C;
}

extern efHal_i2c_ec_t sI2C_transfer(sI2C_dh_t dh, efHal_i2c_devAdd_t da, void *pTx, size_t sTx, void *pRx, size_t sRx)
{
    efHal_i2c_ec_t ret = EF_HAL_I2C_EC_NO_ERROR;
    sI2C_data_t *sI2C_data = dh;
    uint8_t *pBytes;

    /* there aren't reception */
    if (sRx == 0 || pRx == NULL)
    {
        if (pTx == NULL || sTx == 0)
        {
            efErrorHdl_error(EF_ERROR_HDL_INVALID_PARAMETER, "pTx == NULL || sTx == 0");
            ret = EF_HAL_I2C_EC_INVALID_PARAMS;
        }
    }

    if (sTx && ret == EF_HAL_I2C_EC_NO_ERROR)
    {
        pBytes = pTx;

        sendStart(sI2C_data);
        sendByte(sI2C_data, da<<1 | 0);

        if (waitACK(sI2C_data) == 1)
            ret = EF_HAL_I2C_EC_NAK;

        while (sTx && ret == EF_HAL_I2C_EC_NO_ERROR)
        {
            sendByte(sI2C_data, *pBytes);
            sTx--;
            pBytes++;

            if (waitACK(sI2C_data) == 1)
                ret = EF_HAL_I2C_EC_NAK;
        }
    }

    if (sRx && ret == EF_HAL_I2C_EC_NO_ERROR)
    {
        pBytes = pRx;

        sendStart(sI2C_data);
        sendByte(sI2C_data, da<<1 | 1);

        if (waitACK(sI2C_data) == 1)
            ret = EF_HAL_I2C_EC_NAK;

        while (sRx && ret == EF_HAL_I2C_EC_NO_ERROR)
        {
            *pBytes = readByte(sI2C_data);
            sRx--;
            pBytes++;

            if (sRx)
                sendAck(sI2C_data);
            else
                sendNAck(sI2C_data);
        }
    }

    sendStop(sI2C_data);

    efHal_internal_i2c_endOfTransfer(sI2C_data->efHal_dh_I2C, ret);

    return ret;
}

/*==================[end of file]============================================*/
