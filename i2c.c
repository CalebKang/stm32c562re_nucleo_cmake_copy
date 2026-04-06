#include "main.h"
#include "stm32c5xx_hal.h"
#include "vl53lx_api.h"

VL53LX_Dev_t dev;
VL53LX_DEV Dev = &dev;
int status;

void i2c_app(void)
{
  uint8_t byteData;

  /*shutdown*/
  LL_GPIO_ResetOutputPin(SHUT_PORT, SHUT_PIN);
  HAL_Delay(100);
  LL_GPIO_SetOutputPin(SHUT_PORT, SHUT_PIN);
  HAL_Delay(100);

  /*read id*/
  Dev->i2c_slave_address = 0x52;
  VL53LX_RdByte(Dev, 0x010F, &byteData);

  /*ranging*/
  VL53LX_MultiRangingData_t MultiRangingData;
  VL53LX_MultiRangingData_t *pMultiRangingData = &MultiRangingData;
  uint8_t NewDataReady=0;
  int no_of_object_found=0,j;
  printf("Ranging loop starts\n");
  
  status = VL53LX_WaitDeviceBooted(Dev);
  status = VL53LX_DataInit(Dev);
  status = VL53LX_StartMeasurement(Dev);
  
  if(status){
    printf("VL53LX_StartMeasurement failed: error = %d \n", status);
    while(1);
  }

  while(1)
  {
    status = VL53LX_GetMeasurementDataReady(Dev, &NewDataReady);                        

    HAL_Delay(1); // 1 ms polling period, could be longer.

    if((!status)&&(NewDataReady!=0))
    {
      status = VL53LX_GetMultiRangingData(Dev, pMultiRangingData);
      no_of_object_found=pMultiRangingData->NumberOfObjectsFound;
      printf("Count=%5d, ", pMultiRangingData->StreamCount);
      printf("#Objs=%1d ", no_of_object_found);

      for(j=0;j<no_of_object_found;j++)
      {
        if(j!=0)printf("\n                     ");
        printf("status=%d, D=%5dmm, Signal=%2.2f Mcps, Ambient=%2.2f Mcps",
                pMultiRangingData->RangeData[j].RangeStatus,
                pMultiRangingData->RangeData[j].RangeMilliMeter,
                pMultiRangingData->RangeData[j].SignalRateRtnMegaCps/65536.0,
                pMultiRangingData->RangeData[j].AmbientRateRtnMegaCps/65536.0);
      }
      printf ("\n");

      if (status==0)
      {
        status = VL53LX_ClearInterruptAndStartMeasurement(Dev);
      }
    }
  }
}











