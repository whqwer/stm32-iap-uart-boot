#ifndef __IAP_H__
#define __IAP_H__
#include "stm32h503xx.h"
#include "iap_config.h"

/* Exported types ------------------------------------------------------------*/
typedef  void (*pFunction)(void);

extern pFunction Jump_To_Application;
extern uint32_t JumpAddress;

/* IAP Core Functions --------------------------------------------------------*/
extern void IAP_Init(void);
extern int8_t IAP_RunApp(void);
extern int8_t IAP_Update(void);
extern int8_t IAP_Erase(void);

/* Dual-Image Management Functions -------------------------------------------*/
extern ImageConfig_t* IAP_GetConfig(void);
extern void IAP_ConfirmBoot(void);

#endif
