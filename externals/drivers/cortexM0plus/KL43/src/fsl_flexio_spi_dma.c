/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_flexio_spi_dma.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* Component ID definition, used by tools. */
#ifndef FSL_COMPONENT_ID
#define FSL_COMPONENT_ID "platform.drivers.flexio_spi_dma"
#endif

/*<! Structure definition for spi_dma_private_handle_t. The structure is private. */
typedef struct _flexio_spi_master_dma_private_handle
{
    FLEXIO_SPI_Type *base;
    flexio_spi_master_dma_handle_t *handle;
} flexio_spi_master_dma_private_handle_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*!
 * @brief DMA callback function for FLEXIO SPI send transfer.
 *
 * @param handle DMA handle pointer.
 * @param param Callback function parameter.
 */
static void FLEXIO_SPI_TxDMACallback(dma_handle_t *handle, void *param);

/*!
 * @brief DMA callback function for FLEXIO SPI receive transfer.
 *
 * @param handle DMA handle pointer.
 * @param param Callback function parameter.
 */
static void FLEXIO_SPI_RxDMACallback(dma_handle_t *handle, void *param);

/*!
 * @brief EDMA config for FLEXIO SPI transfer.
 *
 * @param base pointer to FLEXIO_SPI_Type structure.
 * @param handle pointer to flexio_spi_master_dma_handle_t structure to store the transfer state.
 * @param xfer Pointer to flexio spi transfer structure.
 */
static void FLEXIO_SPI_DMAConfig(FLEXIO_SPI_Type *base,
                                 flexio_spi_master_dma_handle_t *handle,
                                 flexio_spi_transfer_t *xfer);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/* Dummy data used to send */
static const uint16_t s_dummyData = FLEXIO_SPI_DUMMYDATA;

/*< @brief user configurable flexio spi handle count. */
#define FLEXIO_SPI_HANDLE_COUNT 2

/*<! Private handle only used for internally. */
static flexio_spi_master_dma_private_handle_t s_dmaPrivateHandle[FLEXIO_SPI_HANDLE_COUNT];

/*******************************************************************************
 * Code
 ******************************************************************************/

static void FLEXIO_SPI_TxDMACallback(dma_handle_t *handle, void *param)
{
    flexio_spi_master_dma_private_handle_t *spiPrivateHandle = (flexio_spi_master_dma_private_handle_t *)param;

    /* Disable Tx DMA. */
    FLEXIO_SPI_EnableDMA(spiPrivateHandle->base, (uint32_t)kFLEXIO_SPI_TxDmaEnable, false);

    /* Disable interrupt. */
    DMA_DisableInterrupts(handle->base, handle->channel);

    /* change the state. */
    spiPrivateHandle->handle->txInProgress = false;

    /* All finished, call the callback. */
    if ((spiPrivateHandle->handle->txInProgress == false) && (spiPrivateHandle->handle->rxInProgress == false))
    {
        if (spiPrivateHandle->handle->callback != NULL)
        {
            (spiPrivateHandle->handle->callback)(spiPrivateHandle->base, spiPrivateHandle->handle, kStatus_Success,
                                                 spiPrivateHandle->handle->userData);
        }
    }
}

static void FLEXIO_SPI_RxDMACallback(dma_handle_t *handle, void *param)
{
    flexio_spi_master_dma_private_handle_t *spiPrivateHandle = (flexio_spi_master_dma_private_handle_t *)param;

    /* Disable Rx DMA. */
    FLEXIO_SPI_EnableDMA(spiPrivateHandle->base, (uint32_t)kFLEXIO_SPI_RxDmaEnable, false);

    /* Disable interrupt. */
    DMA_DisableInterrupts(handle->base, handle->channel);

    /* change the state. */
    spiPrivateHandle->handle->rxInProgress = false;

    /* All finished, call the callback. */
    if ((spiPrivateHandle->handle->txInProgress == false) && (spiPrivateHandle->handle->rxInProgress == false))
    {
        if (spiPrivateHandle->handle->callback != NULL)
        {
            (spiPrivateHandle->handle->callback)(spiPrivateHandle->base, spiPrivateHandle->handle, kStatus_Success,
                                                 spiPrivateHandle->handle->userData);
        }
    }
}

static void FLEXIO_SPI_DMAConfig(FLEXIO_SPI_Type *base,
                                 flexio_spi_master_dma_handle_t *handle,
                                 flexio_spi_transfer_t *xfer)
{
    dma_transfer_config_t xferConfig       = {0};
    flexio_spi_shift_direction_t direction = kFLEXIO_SPI_MsbFirst;
    uint8_t bytesPerFrame;

    /* Configure the values in handle. */
    switch (xfer->flags)
    {
        case (uint8_t)kFLEXIO_SPI_8bitMsb:
            bytesPerFrame = 1U;
            direction     = kFLEXIO_SPI_MsbFirst;
            break;
        case (uint8_t)kFLEXIO_SPI_8bitLsb:
            bytesPerFrame = 1U;
            direction     = kFLEXIO_SPI_LsbFirst;
            break;
        case (uint8_t)kFLEXIO_SPI_16bitMsb:
            bytesPerFrame = 2U;
            direction     = kFLEXIO_SPI_MsbFirst;
            break;
        case (uint8_t)kFLEXIO_SPI_16bitLsb:
            bytesPerFrame = 2U;
            direction     = kFLEXIO_SPI_LsbFirst;
            break;
        default:
            bytesPerFrame = 1U;
            direction     = kFLEXIO_SPI_MsbFirst;
            assert(true);
            break;
    }

    /* Save total transfer size. */
    handle->transferSize = xfer->dataSize;

    /* Configure tx transfer DMA. */
    xferConfig.destAddr            = FLEXIO_SPI_GetTxDataRegisterAddress(base, direction);
    xferConfig.enableDestIncrement = false;
    if (bytesPerFrame == 1U)
    {
        xferConfig.srcSize  = kDMA_Transfersize8bits;
        xferConfig.destSize = kDMA_Transfersize8bits;
    }
    else
    {
        if (direction == kFLEXIO_SPI_MsbFirst)
        {
            xferConfig.destAddr -= 1U;
        }
        xferConfig.srcSize  = kDMA_Transfersize16bits;
        xferConfig.destSize = kDMA_Transfersize16bits;
    }

    /* Configure DMA channel. */
    if (xfer->txData != NULL)
    {
        xferConfig.enableSrcIncrement = true;
        xferConfig.srcAddr            = (uint32_t)(xfer->txData);
    }
    else
    {
        /* Disable the source increasement and source set to dummyData. */
        xferConfig.enableSrcIncrement = false;
        xferConfig.srcAddr            = (uint32_t)(&s_dummyData);
    }

    xferConfig.transferSize = xfer->dataSize;

    if (handle->txHandle != NULL)
    {
        (void)DMA_SubmitTransfer(handle->txHandle, &xferConfig, (uint32_t)kDMA_EnableInterrupt);
    }

    /* Configure tx transfer DMA. */
    if (xfer->rxData != NULL)
    {
        xferConfig.srcAddr             = FLEXIO_SPI_GetRxDataRegisterAddress(base, direction);
        xferConfig.enableSrcIncrement  = false;
        xferConfig.destAddr            = (uint32_t)(xfer->rxData);
        xferConfig.enableDestIncrement = true;
        (void)DMA_SubmitTransfer(handle->rxHandle, &xferConfig, (uint32_t)kDMA_EnableInterrupt);
        handle->rxInProgress = true;
        FLEXIO_SPI_EnableDMA(base, (uint32_t)kFLEXIO_SPI_RxDmaEnable, true);
        DMA_StartTransfer(handle->rxHandle);
    }

    /* Always start Tx transfer. */
    if (handle->txHandle != NULL)
    {
        handle->txInProgress = true;
        FLEXIO_SPI_EnableDMA(base, (uint32_t)kFLEXIO_SPI_TxDmaEnable, true);
        DMA_StartTransfer(handle->txHandle);
    }
}

/*!
 * brief Initializes the FLEXO SPI master DMA handle.
 *
 * This function initializes the FLEXO SPI master DMA handle which can be used for other FLEXO SPI master transactional
 * APIs.
 * Usually, for a specified FLEXO SPI instance, call this API once to get the initialized handle.
 *
 * param base Pointer to FLEXIO_SPI_Type structure.
 * param handle Pointer to flexio_spi_master_dma_handle_t structure to store the transfer state.
 * param callback SPI callback, NULL means no callback.
 * param userData callback function parameter.
 * param txHandle User requested DMA handle for FlexIO SPI RX DMA transfer.
 * param rxHandle User requested DMA handle for FlexIO SPI TX DMA transfer.
 * retval kStatus_Success Successfully create the handle.
 * retval kStatus_OutOfRange The FlexIO SPI DMA type/handle table out of range.
 */
status_t FLEXIO_SPI_MasterTransferCreateHandleDMA(FLEXIO_SPI_Type *base,
                                                  flexio_spi_master_dma_handle_t *handle,
                                                  flexio_spi_master_dma_transfer_callback_t callback,
                                                  void *userData,
                                                  dma_handle_t *txHandle,
                                                  dma_handle_t *rxHandle)
{
    assert(handle != NULL);

    uint8_t index = 0;

    /* Find the an empty handle pointer to store the handle. */
    for (index = 0U; index < (uint8_t)FLEXIO_SPI_HANDLE_COUNT; index++)
    {
        if (s_dmaPrivateHandle[index].base == NULL)
        {
            s_dmaPrivateHandle[index].base   = base;
            s_dmaPrivateHandle[index].handle = handle;
            break;
        }
    }

    if (index == (uint8_t)FLEXIO_SPI_HANDLE_COUNT)
    {
        return kStatus_OutOfRange;
    }

    /* Set spi base to handle. */
    handle->txHandle = txHandle;
    handle->rxHandle = rxHandle;

    /* Register callback and userData. */
    handle->callback = callback;
    handle->userData = userData;

    /* Set SPI state to idle. */
    handle->txInProgress = false;
    handle->rxInProgress = false;

    /* Install callback for Tx/Rx dma channel. */
    if (handle->txHandle != NULL)
    {
        DMA_SetCallback(handle->txHandle, FLEXIO_SPI_TxDMACallback, &s_dmaPrivateHandle[index]);
    }
    if (handle->rxHandle != NULL)
    {
        DMA_SetCallback(handle->rxHandle, FLEXIO_SPI_RxDMACallback, &s_dmaPrivateHandle[index]);
    }

    return kStatus_Success;
}

/*!
 * brief Performs a non-blocking FlexIO SPI transfer using DMA.
 *
 * note This interface returned immediately after transfer initiates. Call
 * FLEXIO_SPI_MasterGetTransferCountDMA to poll the transfer status to check
 * whether the FlexIO SPI transfer is finished.
 *
 * param base Pointer to FLEXIO_SPI_Type structure.
 * param handle Pointer to flexio_spi_master_dma_handle_t structure to store the transfer state.
 * param xfer Pointer to FlexIO SPI transfer structure.
 * retval kStatus_Success Successfully start a transfer.
 * retval kStatus_InvalidArgument Input argument is invalid.
 * retval kStatus_FLEXIO_SPI_Busy FlexIO SPI is not idle, is running another transfer.
 */
status_t FLEXIO_SPI_MasterTransferDMA(FLEXIO_SPI_Type *base,
                                      flexio_spi_master_dma_handle_t *handle,
                                      flexio_spi_transfer_t *xfer)
{
    assert(handle != NULL);
    assert(xfer != NULL);

    uint32_t dataMode = 0U;
    uint16_t timerCmp = (uint16_t)base->flexioBase->TIMCMP[base->timerIndex[0]];

    timerCmp &= 0x00FFU;

    /* Check if the device is busy. */
    if ((handle->txInProgress) || (handle->rxInProgress))
    {
        return kStatus_FLEXIO_SPI_Busy;
    }

    /* Check if input parameter invalid. */
    if (((xfer->txData == NULL) && (xfer->rxData == NULL)) || (xfer->dataSize == 0U))
    {
        return kStatus_InvalidArgument;
    }

    /* configure data mode. */
    if ((xfer->flags == (uint8_t)kFLEXIO_SPI_8bitMsb) || (xfer->flags == (uint8_t)kFLEXIO_SPI_8bitLsb))
    {
        dataMode = (8UL * 2UL - 1UL) << 8U;
    }
    else if ((xfer->flags == (uint8_t)kFLEXIO_SPI_16bitMsb) || (xfer->flags == (uint8_t)kFLEXIO_SPI_16bitLsb))
    {
        dataMode = (16UL * 2UL - 1UL) << 8U;
    }
    else
    {
        dataMode = 8UL * 2UL - 1UL;
    }

    dataMode |= timerCmp;

    base->flexioBase->TIMCMP[base->timerIndex[0]] = dataMode;

    FLEXIO_SPI_DMAConfig(base, handle, xfer);

    return kStatus_Success;
}

/*!
 * brief Gets the remaining bytes for FlexIO SPI DMA transfer.
 *
 * param base Pointer to FLEXIO_SPI_Type structure.
 * param handle FlexIO SPI DMA handle pointer.
 * param count Number of bytes transferred so far by the non-blocking transaction.
 */
status_t FLEXIO_SPI_MasterTransferGetCountDMA(FLEXIO_SPI_Type *base,
                                              flexio_spi_master_dma_handle_t *handle,
                                              size_t *count)
{
    assert(handle != NULL);

    if (NULL == count)
    {
        return kStatus_InvalidArgument;
    }

    if (handle->rxInProgress)
    {
        *count = (handle->transferSize - DMA_GetRemainingBytes(handle->rxHandle->base, handle->rxHandle->channel));
    }
    else
    {
        *count = (handle->transferSize - DMA_GetRemainingBytes(handle->txHandle->base, handle->txHandle->channel));
    }

    return kStatus_Success;
}

/*!
 * brief Aborts a FlexIO SPI transfer using DMA.
 *
 * param base Pointer to FLEXIO_SPI_Type structure.
 * param handle FlexIO SPI DMA handle pointer.
 */
void FLEXIO_SPI_MasterTransferAbortDMA(FLEXIO_SPI_Type *base, flexio_spi_master_dma_handle_t *handle)
{
    assert(handle != NULL);

    /* Disable dma. */
    DMA_AbortTransfer(handle->txHandle);
    DMA_AbortTransfer(handle->rxHandle);

    /* Disable DMA enable bit. */
    FLEXIO_SPI_EnableDMA(base, (uint32_t)kFLEXIO_SPI_DmaAllEnable, false);

    /* Set the handle state. */
    handle->txInProgress = false;
    handle->rxInProgress = false;
}

/*!
 * brief Performs a non-blocking FlexIO SPI transfer using DMA.
 *
 * note This interface returns immediately after transfer initiates. Call
 * FLEXIO_SPI_SlaveGetTransferCountDMA to poll the transfer status and
 * check whether the FlexIO SPI transfer is finished.
 *
 * param base Pointer to FLEXIO_SPI_Type structure.
 * param handle Pointer to flexio_spi_slave_dma_handle_t structure to store the transfer state.
 * param xfer Pointer to FlexIO SPI transfer structure.
 * retval kStatus_Success Successfully start a transfer.
 * retval kStatus_InvalidArgument Input argument is invalid.
 * retval kStatus_FLEXIO_SPI_Busy FlexIO SPI is not idle, is running another transfer.
 */
status_t FLEXIO_SPI_SlaveTransferDMA(FLEXIO_SPI_Type *base,
                                     flexio_spi_slave_dma_handle_t *handle,
                                     flexio_spi_transfer_t *xfer)
{
    assert(handle != NULL);
    assert(xfer != NULL);

    uint32_t dataMode = 0U;

    /* Check if the device is busy. */
    if ((handle->txInProgress) || (handle->rxInProgress))
    {
        return kStatus_FLEXIO_SPI_Busy;
    }

    /* Check if input parameter invalid. */
    if (((xfer->txData == NULL) && (xfer->rxData == NULL)) || (xfer->dataSize == 0U))
    {
        return kStatus_InvalidArgument;
    }

    /* configure data mode. */
    if ((xfer->flags == (uint8_t)kFLEXIO_SPI_8bitMsb) || (xfer->flags == (uint8_t)kFLEXIO_SPI_8bitLsb))
    {
        dataMode = 8UL * 2UL - 1UL;
    }
    else if ((xfer->flags == (uint8_t)kFLEXIO_SPI_16bitMsb) || (xfer->flags == (uint8_t)kFLEXIO_SPI_16bitLsb))
    {
        dataMode = 16UL * 2UL - 1UL;
    }
    else
    {
        dataMode = 8UL * 2UL - 1UL;
    }

    base->flexioBase->TIMCMP[base->timerIndex[0]] = dataMode;

    FLEXIO_SPI_DMAConfig(base, handle, xfer);

    return kStatus_Success;
}
