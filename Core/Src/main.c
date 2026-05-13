#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_flash.h"

#define BLINK_MIN 4
#define BLINK_MAX 7

#define FLASH_BLINK_ADDR  0x0800FC00U
#define FLASH_PAGE_SIZE   0x400U

#define FACTORY_RESET_SEC 3

volatile uint8_t blink_count = BLINK_MIN;
volatile uint8_t current_blink = 0;
volatile uint8_t pause_counter = 0;
volatile uint8_t is_pausing = 0;
volatile uint32_t last_button_time = 0;
volatile uint8_t ignore_next_release = 0;

void SystemClock_Config(void);
void Flash_WriteBlink(uint8_t value);
uint8_t Flash_ReadBlink(void);

uint8_t Flash_ReadBlink(void)
{
    uint16_t val = *(__IO uint16_t *)FLASH_BLINK_ADDR;

    if (val == 0xFFFF || val < BLINK_MIN || val > BLINK_MAX)
    {
        Flash_WriteBlink(BLINK_MIN);
        return BLINK_MIN;
    }

    return (uint8_t)val;
}

void Flash_WriteBlink(uint8_t value)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0;

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = FLASH_BLINK_ADDR;
    erase.NbPages     = 1;

    HAL_FLASHEx_Erase(&erase, &page_error);

    HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_HALFWORD,
        FLASH_BLINK_ADDR,
        (uint16_t)value
    );

    HAL_FLASH_Lock();
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1;

    GPIOC->ODR |= GPIO_ODR_ODR13;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOA->CRL |= GPIO_CRL_CNF0_1;

    GPIOA->ODR |= GPIO_ODR_ODR0;

    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOA->CRL |= GPIO_CRL_MODE1_1;

    GPIOA->ODR &= ~GPIO_ODR_ODR1;

    blink_count = Flash_ReadBlink();

    if (!(GPIOA->IDR & GPIO_IDR_IDR0))
    {
        uint32_t start = HAL_GetTick();

        while (!(GPIOA->IDR & GPIO_IDR_IDR0))
        {
            if ((HAL_GetTick() - start) >= (FACTORY_RESET_SEC * 1000))
            {
                blink_count = BLINK_MIN;

                Flash_WriteBlink(BLINK_MIN);

                ignore_next_release = 1;

                break;
            }
        }
    }

    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
    AFIO->EXTICR[0] |= AFIO_EXTICR1_EXTI0_PA;

    EXTI->IMR  |= EXTI_IMR_MR0;
    EXTI->FTSR |= EXTI_FTSR_TR0;
    EXTI->RTSR |= EXTI_RTSR_TR0;

    NVIC_EnableIRQ(EXTI0_IRQn);

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->PSC = 7999;
    TIM2->ARR = 999;

    TIM2->DIER |= TIM_DIER_UIE;
    TIM2->CR1  |= TIM_CR1_CEN;

    NVIC_EnableIRQ(TIM2_IRQn);

    while (1)
    {

    }
}

void EXTI0_IRQHandler(void)
{
    EXTI->PR |= EXTI_PR_PR0;

    uint32_t now = HAL_GetTick();

    if ((now - last_button_time) < 5000)
        return;

    if (GPIOA->IDR & GPIO_IDR_IDR0)
    {
        if (ignore_next_release)
        {
            ignore_next_release = 0;
            last_button_time = now;
            return;
        }

        if (blink_count >= BLINK_MAX)
            blink_count = BLINK_MIN;
        else
            blink_count++;

        Flash_WriteBlink(blink_count);

        current_blink = 0;
        pause_counter = 0;
        is_pausing = 0;

        GPIOC->ODR |= GPIO_ODR_ODR13;

        last_button_time = now;
    }
}

void TIM2_IRQHandler(void)
{
    TIM2->SR &= ~TIM_SR_UIF;

    if (is_pausing)
    {
        pause_counter++;

        if (pause_counter >= 5)
        {
            pause_counter = 0;
            current_blink = 0;
            is_pausing = 0;
        }
    }
    else
    {
        if (current_blink < blink_count * 2)
        {
            GPIOC->ODR ^= GPIO_ODR_ODR13;
            current_blink++;
        }
        else
        {
            GPIOC->ODR |= GPIO_ODR_ODR13;

            is_pausing = 1;
            pause_counter = 0;
        }
    }
}

void SystemClock_Config(void)
{

}