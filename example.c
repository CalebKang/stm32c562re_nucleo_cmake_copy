#include "main.h"
#include "stm32c5xx_ll_utils.h"
#include "example.h"


#define ADC_DELAY_CALIB_ENABLE_CPU_CYCLES (LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES * 4U)
#define ADC_BUFFER_SIZE (5)

uint16_t ADCRawData[ADC_BUFFER_SIZE];
uint32_t ADC1_DMA_LLI_BUF[LL_DMA_NODE_REGISTER_NUM];


int _write(int file, char *ptr, int len)
{
  for(int i=0; i<len; i++)
  {
    while((LL_USART_READ_REG(USART2, ISR) & LL_USART_ISR_TXE_TXFNF) == 0){};
    LL_USART_TransmitData8(USART2, ptr[i]);
  }
  return len;
}


static inline void DelayUs(uint32_t delay_us)
{
  volatile uint32_t wait_loop_index = ((delay_us * (SystemCoreClock >> 19U)) >> 2U);
  while (wait_loop_index != 0U)
  {
    wait_loop_index--;
  }
}


void app_init(void)
{
    /* adc */
    uint32_t wait_loop_index = (ADC_DELAY_CALIB_ENABLE_CPU_CYCLES >> 1U);

    LL_ADC_Disable(ADC1);
    LL_ADC_ClearFlag_ADRDY(ADC1);
    while (LL_ADC_IsEnabled(ADC1) != 0U)
    {
    }
    LL_ADC_DisableDeepPowerDown(ADC1);
    LL_ADC_EnableInternalRegulator(ADC1);
    DelayUs(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
    LL_ADC_StartCalibration(ADC1, LL_ADC_IN_SINGLE_ENDED);
    while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0U)
    {
    }

    while (wait_loop_index != 0U)
    {
      wait_loop_index--;
    }

    LL_ADC_ClearFlag_ADRDY(ADC1);
    LL_ADC_Enable(ADC1);
    while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0U)
    {
      if (LL_ADC_IsEnabled(ADC1) == 0U)
      {
        LL_ADC_Enable(ADC1);
      }
    }

    LL_Delay_NoISR(10);

    /*start adc dma*/
    LL_DMA_ConfigAddresses(LL_LPDMA1_CH0, LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA), (uint32_t)ADCRawData);
    LL_DMA_SetBlkDataLength(LL_LPDMA1_CH0, ADC_BUFFER_SIZE * 2);
    LL_DMA_ClearFlag(LL_LPDMA1_CH0, LL_DMA_FLAG_ALL);
    LL_DMA_EnableIT(LL_LPDMA1_CH0, LL_DMA_IT_TC);
    LL_DMA_SetLinkedListBaseAddr(LPDMA1_CH0, (uint32_t)&ADC1_DMA_LLI_BUF);
    LL_DMA_ConfigLinkUpdate(LPDMA1_CH0, LL_DMA_UPDATE_ALL, (uint32_t)&ADC1_DMA_LLI_BUF);

    ADC1_DMA_LLI_BUF[LL_DMA_NODE_CTR1_REG_OFFSET] = LL_DMA_SRC_ADDR_FIXED | LL_DMA_SRC_DATA_WIDTH_HALFWORD | LL_DMA_DEST_ADDR_INCREMENTED | LL_DMA_DEST_DATA_WIDTH_HALFWORD;
    ADC1_DMA_LLI_BUF[LL_DMA_NODE_CTR2_REG_OFFSET] = LL_LPDMA1_REQUEST_ADC1 | LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
    ADC1_DMA_LLI_BUF[LL_DMA_NODE_CLLR_REG_OFFSET] = LL_DMA_UPDATE_ALL | ((uint32_t)&ADC1_DMA_LLI_BUF & DMA_CLLR_LA);
    ADC1_DMA_LLI_BUF[LL_DMA_NODE_CBR1_REG_OFFSET] = LL_DMA_READ_REG(LL_LPDMA1_CH0, CBR1);
    ADC1_DMA_LLI_BUF[LL_DMA_NODE_CSAR_REG_OFFSET] = LL_DMA_READ_REG(LL_LPDMA1_CH0, CSAR);
    ADC1_DMA_LLI_BUF[LL_DMA_NODE_CDAR_REG_OFFSET] = LL_DMA_READ_REG(LL_LPDMA1_CH0, CDAR);

    LL_DMA_EnableChannel(LL_LPDMA1_CH0);
    LL_ADC_REG_SetDataTransferMode(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);
    LL_ADC_REG_StartConversion(ADC1);


    /* timer1 */
    LL_TIM_OC_SetCompareCH1(TIM1, 0x800);
    LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH1);
    LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH1N);
    LL_TIM_EnableAllOutputs(TIM1);
    LL_TIM_SetAutoReload(TIM1, 0x1000);
    LL_TIM_EnableCounter(TIM1);
    LL_TIM_EnableIT_UPDATE(TIM1);

    printf("start..\r\n");
    
    i2c_app();

    while(1)
    {
    }
}


void TIM1_UPD_IRQHandler(void)
{
  if(LL_TIM_IsActiveFlag_UPDATE(TIM1) == 1)
  {
    LL_TIM_ClearFlag_UPDATE(TIM1);
  }

}


void LPDMA1_CH0_IRQHandler(void)
{
#if 1
  LL_GPIO_SetOutputPin(TEST_DMA_TC_PORT, TEST_DMA_TC_PIN);
  LL_GPIO_ResetOutputPin(TEST_DMA_TC_PORT, TEST_DMA_TC_PIN);

  if (LL_DMA_IsActiveFlag_MIS(LPDMA1, LL_DMA_CHANNEL_0) == 0U)
  {
    return;
  }

  if(LL_DMA_IsActiveFlag_TC(LL_LPDMA1_CH0) == 1)
  {
    LL_DMA_ClearFlag_TC(LL_LPDMA1_CH0);
  }
#else
  uint32_t flags;
  uint32_t its;

  flags = LL_DMA_READ_REG(LL_LPDMA1_CH0, CSR);
  its = LL_DMA_READ_REG(LL_LPDMA1_CH0, CCR);

  /* Transfer Complete Interrupt management */
  if (STM32_READ_BIT((flags & its), LL_DMA_FLAG_TC) != 0U)
  {
    LL_DMA_ClearFlag_TC(LL_LPDMA1_CH0);
  }
#endif
}








