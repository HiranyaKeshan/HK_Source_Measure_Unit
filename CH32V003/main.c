/*
 *      HK Source Measure Unit
 *      Designed & Developed by Hiranya Keshan
 */

#include "debug.h"

#define LATCH_PORT  GPIOD
#define LATCH_PIN   GPIO_Pin_0      // ST_CP (latch)

#define CLK_PORT    GPIOC
#define CLK_PIN     GPIO_Pin_0      // SH_CP (shift clock)

#define DATA_PORT   GPIOD
#define DATA_PIN    GPIO_Pin_4      // DS (serial data)

#define MUX_PORT    GPIOC

#define D1_PIN      GPIO_Pin_7      // D1
#define D2_PIN      GPIO_Pin_6      //D2
#define D3_PIN      GPIO_Pin_5      //D3

#define IADC        0x90
#define VADC        0x92

#define FDAC        0xC0
#define CDAC        0xC2

#define Ialert      EXTI_Line3
#define Valert      EXTI_Line2

#define NUM_CHANNELS     2
#define SAMPLES_PER_SET  1
#define BUFFER_SIZE     (NUM_CHANNELS * SAMPLES_PER_SET)

u16 TxBuf[BUFFER_SIZE];

/*
 * USART Protocol:
 *
 *  |       |       |       |       |       |       |       |       |
 *  |   7   |   6   |   5   |   4   |   3   |   2   |   1   |   0   |
 *  |_______|_______|_______|_______|_______|_______|_______|_______|
 *  |       |                       |                               |
 *  |  EOD  |      Identifier       |           Data Bits           |
 *  |_______|_______________________|_______ _______ _______ _______|
 *  |       |       |       |       |       |       |       |       |
 *
 *  EOD(End of data) - If 0 -> End of the continous massage stream
 *                     If 1 -> Continous massage stream
 *
 *  Identifier - 000 -> FDAC
 *               001 -> CDAC
 *               010 -> IADC
 *               011 -> VADC
 *               100 -> Istage
 *               101 -> Vstage
 *               110 -> Misc Addr
 *               111 -> Misc Data
 *
 *  Data Bits  - Data transmission bits
 *               Under "Special" identifier,
 *               0000 -> MUX pos1
 *               0001 -> MUX pos2
 *               0010 -> MUX pos3
 *               0011 -> MUX pos4
 *               0100 ->
 *               0101 ->
 *               0110 ->
 *               0111 ->
 *               1000 ->
 *               1001 ->
 *               1010 ->
 *               1011 ->
 *               1100 -> System Activate
 *               1101 -> Sample Rate
 *               1110 -> Sleep Mode
 *               1111 -> Reset
 */

const int USART_FDAC = 0x0;
const int USART_CDAC = 0x1;
const int USART_IADC = 0x2;
const int USART_VADC = 0x3;
const int USART_ISTAGE = 0x4;
const int USART_VSTAGE = 0x5;
const int USART_MADDR = 0x6;
const int USART_MDATA = 0x7;

const int CMD_MUX_POS1 = 0x0;
const int CMD_MUX_POS2 = 0x1;
const int CMD_MUX_POS3 = 0x2;
const int CMD_MUX_POS4 = 0x3;
const int CMD_MUX_0FF = 0x4;

//Unassigned
const int CMD_CODE_4 = 0x4;
const int CMD_CODE_5 = 0x5;
const int CMD_CODE_6 = 0x6;
const int CMD_CODE_7 = 0x7;
const int CMD_CODE_8 = 0x8;
const int CMD_CODE_9 = 0x9;
const int CMD_CODE_A = 0xA;

const int CMD_POSITIVE = 0xB;
const int CMD_OUTPUT_ENABLE = 0xC;
const int CMD_SAMPLE_RATE = 0xD;
const int CMD_SLEEP_MODE = 0xE;
const int CMD_RESET = 0xF;

//  D1 -> E     D2 -> S0        D3 -> S1
const int MUX_P_CLAMP = 0x0;
const int MUX_N_CLAMP = 0x3;
const int MUX_FDAC = 0x0;
const int MUX_CDAC = 0x3;
const int MUX_VMID = 0x1;
const int MUX_REF = 0x2;
const int MUX_OFF = 0x7;

volatile uint16_t CUR_FDAC = 0x00;
volatile uint16_t CUR_CDAC = 0x00;

volatile uint8_t CUR_MADDR = 0x00;
volatile uint8_t CUR_MDATA = 0x00;

volatile uint8_t RX_State = 0;
volatile uint8_t RX_ID = 0;
volatile uint16_t RX_Data = 0;

volatile uint8_t rx_buffer[2];
volatile uint8_t rx_index = 0;
volatile uint16_t received_value = 0;
volatile uint8_t packet_ready = 0;

volatile uint8_t Positive = SET;        //SET -> Positive , RESET -> Negative
volatile uint8_t Cycle = RESET;

uint8_t i;
uint8_t Auto_Range =RESET;
volatile uint8_t Active = 0;
volatile uint8_t Istage = 5;
volatile uint8_t Vstage = 0;
volatile uint8_t Stage_mask = 0;
volatile u16 Ival = 0;
volatile u16 Vval = 0;

static const uint16_t P_Vstage_Up[3] = {
    0x7F0,
    0x7F0,
    0x7F0
};

static const uint16_t P_Vstage_Down[3] = {
    0x7E0,
    0x7E0,
    0x7E0
};

static const uint16_t P_Istage_Up[6] = {
    0x7F0,
    0x7F0,
    0x7F0,
    0x7F0,
    0x7F0,
    0x7F0
};

static const uint16_t P_Istage_Down[6] = {
    0x7E0,
    0x7E0,
    0x7E0,
    0x7E0,
    0x7E0,
    0x7E0
};

static const uint16_t N_Vstage_Up[3] = {
    0x80F,
    0x80F,
    0x80F
};

static const uint16_t N_Vstage_Down[3] = {
    0x81F,
    0x81F,
    0x81F
};

static const uint16_t N_Istage_Up[6] = {
    0x80F,
    0x80F,
    0x80F,
    0x80F,
    0x80F,
    0x80F
};

static const uint16_t N_Istage_Down[6] = {
    0x81F,
    0x81F,
    0x81F,
    0x81F,
    0x81F,
    0x81F
};

static const uint8_t Istage_Mask[6] = {
    0x1F,
    0x1B,
    0x13,
    0x03,
    0x02,
    0x00
};

static const uint8_t Vstage_Mask[3] = {
    0x40,
    0x80,
    0x20
};

void GPIO_Toggle_INIT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void Shift_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /* LATCH */
    GPIO_InitStructure.GPIO_Pin = LATCH_PIN;
    GPIO_Init(LATCH_PORT, &GPIO_InitStructure);

    /* CLK */
    GPIO_InitStructure.GPIO_Pin = CLK_PIN;
    GPIO_Init(CLK_PORT, &GPIO_InitStructure);

    /* DATA */
    GPIO_InitStructure.GPIO_Pin = DATA_PIN;
    GPIO_Init(DATA_PORT, &GPIO_InitStructure);

    GPIO_ResetBits(LATCH_PORT, LATCH_PIN);
    GPIO_ResetBits(CLK_PORT,   CLK_PIN);
    GPIO_ResetBits(DATA_PORT,  DATA_PIN);
}

void MUX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /* D1 */
    GPIO_InitStructure.GPIO_Pin = D1_PIN;
    GPIO_Init(MUX_PORT, &GPIO_InitStructure);

    /* D2 */
    GPIO_InitStructure.GPIO_Pin = D2_PIN;
    GPIO_Init(MUX_PORT, &GPIO_InitStructure);

    /* D3 */
    GPIO_InitStructure.GPIO_Pin = D3_PIN;
    GPIO_Init(MUX_PORT, &GPIO_InitStructure);

    GPIO_ResetBits(MUX_PORT, D1_PIN);
    GPIO_ResetBits(MUX_PORT, D2_PIN);
    GPIO_ResetBits(MUX_PORT, D3_PIN);
}

void IIC_Init(u32 bound, u16 address)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef I2C_InitTSturcture = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    I2C_InitTSturcture.I2C_ClockSpeed = bound;
    I2C_InitTSturcture.I2C_Mode = I2C_Mode_I2C;
    I2C_InitTSturcture.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitTSturcture.I2C_OwnAddress1 = address;
    I2C_InitTSturcture.I2C_Ack = I2C_Ack_Enable;
    I2C_InitTSturcture.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitTSturcture);

    I2C_Cmd(I2C1, ENABLE);
}

void ADS1015_Regsel(uint8_t addr , uint8_t pointer)
{
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET);

    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    // Send ADS1015 address + write bit (0x90)
    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, pointer);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
}

void ADS1015_Comm(uint8_t addr , uint8_t regsel , uint8_t data1 , uint8_t data2)
{
    ADS1015_Regsel(addr , regsel);

    I2C_SendData(I2C1, data1);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    I2C_SendData(I2C1, data2);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C1, ENABLE);
}

void ADS1015_SetConv(uint8_t addr)
{
    ADS1015_Regsel(addr , 0x00);
    I2C_GenerateSTOP(I2C1, ENABLE);
}

u16 ADS1015_ReadData(uint8_t addr)
{
    u8 temp1 = 0;
    u8 temp2 = 0;
    u16 temp = 0;

    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET);

    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    // Send ADS1015 address + read bit (0x91)
    I2C_Send7bitAddress(I2C1, addr | 0x01, I2C_Direction_Receiver);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    // Read first byte (MSB)
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
    temp1 = I2C_ReceiveData(I2C1);

    // Read second byte (LSB)
    //I2C_AcknowledgeConfig(I2C1, DISABLE); // Send NACK for last byte
    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) == RESET);
    temp2 = I2C_ReceiveData(I2C1);

    I2C_GenerateSTOP(I2C1, ENABLE);

    temp = (temp1 << 4) | (temp2 >> 4);
    return temp;
}

void Set_DAC(uint8_t addr , u16 val)
{
    uint8_t b0 = (val >> 8) & 0x0F;
    uint8_t b1 = val & 0xFF;

    while(I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET);

    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, b0);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C1, b1);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C1, ENABLE);
}

void ADC_Function_Init(void)
{
    ADC_InitTypeDef  ADC_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    ADC_DeInit(ADC1);

    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = ENABLE;              // Important for multi-channel
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = NUM_CHANNELS;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* Channel sequence: first CH2 (PD2), then CH3 (PD3) */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 1, ADC_SampleTime_241Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 2, ADC_SampleTime_241Cycles);

    ADC_Calibration_Vol(ADC1, ADC_CALVOL_50PERCENT);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    /* Calibration */
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

void DMA_Tx_Init(DMA_Channel_TypeDef *DMA_CHx, u32 ppadr, u32 memadr, u16 bufsize)
{
    DMA_InitTypeDef DMA_InitStructure = {0};

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA_CHx);

    DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
    DMA_InitStructure.DMA_MemoryBaseAddr     = memadr;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = bufsize;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;   // Continuous sampling
    DMA_InitStructure.DMA_Priority           = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);
}

//void EXTI0_INT_INIT(void)
//{
//    GPIO_InitTypeDef GPIO_InitStructure = {0};
//    EXTI_InitTypeDef EXTI_InitStructure1 = {0};
//    EXTI_InitTypeDef EXTI_InitStructure2 = {0};
//    NVIC_InitTypeDef NVIC_InitStructure = {0};
//
//    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
//
//    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
//    GPIO_Init(GPIOC, &GPIO_InitStructure);
//
//    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource3);
//    EXTI_InitStructure1.EXTI_Line = EXTI_Line3;
//    EXTI_InitStructure1.EXTI_Mode = EXTI_Mode_Interrupt;
//    EXTI_InitStructure1.EXTI_Trigger = EXTI_Trigger_Falling;
//    EXTI_InitStructure1.EXTI_LineCmd = ENABLE;
//    EXTI_Init(&EXTI_InitStructure1);
//
//    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource4);
//    EXTI_InitStructure2.EXTI_Line = EXTI_Line4;
//    EXTI_InitStructure2.EXTI_Mode = EXTI_Mode_Interrupt;
//    EXTI_InitStructure2.EXTI_Trigger = EXTI_Trigger_Falling;
//    EXTI_InitStructure2.EXTI_LineCmd = ENABLE;
//    EXTI_Init(&EXTI_InitStructure2);
//
//    NVIC_InitStructure.NVIC_IRQChannel = EXTI7_0_IRQn;
//    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
//    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
//    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//    NVIC_Init(&NVIC_InitStructure);
//}

void TIM2_ms_Init(uint16_t ms)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    uint32_t prescaler = 479;        // 48MHz / 480 = 100 kHz -> 0.01 ms per tick
    uint32_t period    = (ms * 100) - 1;  // e.g. 20ms -> 2000-1 = 1999

    TIM_TimeBaseInitTypeDef t = {0};
    t.TIM_Period        = period;        // 1999 -> interrupt every 20.000 ms
    t.TIM_Prescaler     = prescaler;     // 479
    t.TIM_ClockDivision = TIM_CKD_DIV1;
    t.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &t);

    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitTypeDef n = {0};
    n.NVIC_IRQChannel = TIM2_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 0;
    n.NVIC_IRQChannelSubPriority = 1;
    n.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&n);

    TIM_Cmd(TIM2, ENABLE);
}

void ShiftOut(void)
{
    Stage_mask = Istage_Mask[Istage] | Vstage_Mask[Vstage];

    GPIO_ResetBits(LATCH_PORT, LATCH_PIN);

    for (i = 0; i < 8; ++i)
    {
        GPIO_WriteBit(DATA_PORT, DATA_PIN, (Stage_mask >> (7-i)) & 0x01);
        Delay_Us(100);
        GPIO_SetBits(CLK_PORT, CLK_PIN);
        Delay_Us(100);
        GPIO_ResetBits(CLK_PORT, CLK_PIN);
    }

    GPIO_SetBits(LATCH_PORT, LATCH_PIN);
    Delay_Us(100);
    GPIO_ResetBits(LATCH_PORT, LATCH_PIN);
}

void MUXOut(uint8_t val)
{
    GPIOC->OUTDR = (GPIOC->OUTDR & ~(0x7U << 5)) | (val << 5);
}

void SetRange(void)
{
    if (Positive)
    {
        if ((Istage < 6) & (Ival < 0x7FF))
        {
            if (Ival >= P_Istage_Up[Istage])
            {
                Istage++;
            }
        }
        else if ((Istage >= 0) & (Ival <= P_Istage_Down[Istage]))
        {
            Istage--;
        }

        if ((Vstage < 3) & (Vval < 0x7FF))
        {
            if (Vval >= P_Vstage_Up[Vstage])
            {
                Vstage++;
            }
        }
        else if ((Vstage >= 0) & (Vval <= P_Vstage_Down[Vstage]))
        {
            Vstage--;
        }

    } else
    {
        if ((Istage < 6) & (Ival > 0x7FF))
        {
            if (Ival <= N_Istage_Up[Istage])
            {
                Istage++;
            }
        }
        else if ((Istage >= 0) & (Ival >= N_Istage_Down[Istage]))
        {
            Istage--;
        }

        if ((Vstage < 6) & (Vval > 0x7FF))
        {
            if (Vval >= N_Vstage_Up[Vstage])
            {
                Vstage++;
            }
        }
        else if ((Vstage >= 0) & (Vval <= N_Vstage_Down[Vstage]))
        {
            Vstage--;
        }
    }
}

void USART1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 921600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);
}

__attribute__((always_inline)) static inline
void USART_SendPacket_12bit(uint8_t id, uint16_t data)
{
    uint8_t hdr = (id & 7) << 4;

    register uint8_t b0 = hdr | 0x80 | (data >> 8);
    register uint8_t b1 = hdr | 0x80 | ((data >> 4) & 0x0F);
    register uint8_t b2 = hdr | (data & 0x0F);

    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, b0);

    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, b1);

    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, b2);
}

__attribute__((always_inline)) static inline
void USART_SendPacket_4bit(uint8_t id, uint8_t data)
{
    uint8_t hdr = (id & 7) << 4;

    register uint8_t b0 = hdr | (data & 0x0F);

    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, b0);
}

void RX_Action(uint8_t hid, uint16_t hdata)
{
    switch (hid)
    {
        case USART_FDAC:
            CUR_FDAC = hdata;
            break;

        case USART_CDAC:
            CUR_CDAC = hdata;
            break;

        case USART_IADC:
            break;

        case USART_VADC:
            break;

        case USART_ISTAGE:
            break;

        case USART_VSTAGE:
            break;

        case USART_MADDR:
            CUR_MADDR = hdata;

            switch (CUR_MADDR)
            {
                case CMD_POSITIVE:
                    Positive = CUR_MDATA;
                    USART_SendPacket_4bit(USART_MDATA, Positive);
                    break;

                case CMD_OUTPUT_ENABLE:
                    Active = CUR_MDATA;
                    Set_DAC(FDAC, 0);
                    Set_DAC(CDAC, 0);
                    break;
            }
            break;

        case USART_MDATA:
            CUR_MDATA = hdata;
            break;
    }
}

void Active_Function(void)
{
    __disable_irq();
    Ival += ADS1015_ReadData(IADC);
    Vval += ADS1015_ReadData(VADC);

    //ShiftOut();
    if (Auto_Range) {
        SetRange();
    }

    USART_SendPacket_12bit(USART_IADC, Ival);
    USART_SendPacket_12bit(USART_VADC, Vval);
    USART_SendPacket_4bit(USART_ISTAGE, Istage);
    USART_SendPacket_4bit(USART_VSTAGE, Vstage);
    USART_SendPacket_4bit(USART_MADDR, CUR_MADDR);
    USART_SendPacket_4bit(USART_MDATA, CUR_MDATA);
//    Set_DAC(FDAC, CUR_FDAC);
//    Set_DAC(CDAC, CUR_CDAC);
    __enable_irq();
}

int main(void)
{
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    Delay_Init();
    USART1_Init();

    SDI_Printf_Enable();
    printf("SystemClk:%d\r\n", SystemCoreClock);

    ADC_Function_Init();
    GPIO_Toggle_INIT();
    Shift_GPIO_Init();
    MUX_GPIO_Init();
    IIC_Init(400000, 0x02);

    ShiftOut();
    MUXOut(CMD_MUX_POS1);

    Set_DAC(FDAC, 10);
    Set_DAC(CDAC, 2500);

    //Lo_thresh Register
    ADS1015_Comm(IADC , 0x02 , 0x00 , 0x00);
    //Hi_thresh Register
    ADS1015_Comm(IADC , 0x03 , 0xFF , 0xFF);
    //Config Register
    ADS1015_Comm(IADC, 0x01 , 0x86 , 0xE8);
    ADS1015_SetConv(IADC);

    //Lo_thresh Register
    ADS1015_Comm(VADC , 0x02 , 0x00 , 0x00);
    //Hi_thresh Register
    ADS1015_Comm(VADC , 0x03 , 0xFF , 0xFF);
    //Config Register
    ADS1015_Comm(VADC, 0x01 , 0x86 , 0xE8);
    ADS1015_SetConv(VADC);

    //EXTI0_INT_INIT();
    TIM2_ms_Init(20);
    Delay_Ms(100);

    DMA_Tx_Init(DMA1_Channel1, (u32)&ADC1->RDATAR, (u32)TxBuf, BUFFER_SIZE);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    while(1)
    {
        int count = 40;
        for(int k = 0; k < count; k++){
            Active_Function();
            Delay_Ms(1);
        }
        printf("IADC = %d       ",Ival/count);
        printf("VADC = %d",Vval/count);
//        Ival = 0;
//        Vval = 0;

//        if (Cycle)
//        {
//            Active_Function();
//            Cycle = RESET;
//        }
    }
}

//void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
//
//void EXTI7_0_IRQHandler(void)
//{
//    if(EXTI_GetITStatus(EXTI_Line3)!=RESET)
//    {
//        Ival = ADS1015_ReadData(IADC);
//
//        EXTI_ClearITPendingBit(EXTI_Line3);
//    }
//
//    if(EXTI_GetITStatus(EXTI_Line4)!=RESET)
//    {
//        Ival = ADS1015_ReadData(VADC);
//
//        EXTI_ClearITPendingBit(EXTI_Line4);
//    }
//}

void TIM2_IRQHandler(void) __attribute__((interrupt));
void TIM2_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        Cycle = SET;
    }
}

void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART1);
        uint16_t payload = byte & 0x000F;
        uint8_t id   = (byte >> 4) & 0x07;

        if (byte & 0x80)
        {
            if (RX_State == 0)
            {
                RX_Data = payload << 8;
                RX_State = 1;
            }
            else
            {
                RX_Data &= 0x0F00;
                RX_Data |= (payload << 4);
            }
        }
        else
        {
            RX_ID = id;
            RX_Data |= payload;
            RX_Action(RX_ID, RX_Data);
            RX_State = RESET;
            RX_Data = RESET;
        }
    }
}
