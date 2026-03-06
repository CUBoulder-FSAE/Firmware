#ifndef VCU_ADC_H
#define VCU_ADC_H

void VCU_ADC_Init(void);
uint16_t VCU_ADC_ReadRaw(void);
float VCU_ADC_ReadVoltage(void);

#endif