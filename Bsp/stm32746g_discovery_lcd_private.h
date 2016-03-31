#pragma once

#ifdef __cplusplus
  extern "C" {
#endif

extern LTDC_HandleTypeDef hLtdc;
void LCD_LTDC_IRQHandler();
void LCD_DMA2D_IRQHandler();

#ifdef __cplusplus
   }
#endif
