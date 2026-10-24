/*
 * Logical pin map for the rev A board. Pin choices avoid the LTDC, FMC and
 * QUADSPI alternate functions on the STM32H743 LQFP208/TFBGA240 package;
 * update together with the .ioc when the schematic changes.
 */
#include "pal_gpio.h"
#include "port_stm32h7.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    bool          output;
    bool          init_level;
} pin_map_t;

static const pin_map_t k_pins[PAL_PIN_COUNT] = {
    [PAL_PIN_POWER_HOLD]   = { GPIOK, GPIO_PIN_3,  true,  true  },
    [PAL_PIN_IGN_SENSE]    = { GPIOK, GPIO_PIN_4,  false, false },
    [PAL_PIN_BACKLIGHT_EN] = { GPIOK, GPIO_PIN_5,  true,  false },
    [PAL_PIN_LCD_RESET]    = { GPIOK, GPIO_PIN_6,  true,  false },
    [PAL_PIN_TOUCH_RESET]  = { GPIOH, GPIO_PIN_2,  true,  false },
    [PAL_PIN_TOUCH_INT]    = { GPIOH, GPIO_PIN_3,  false, false },
    [PAL_PIN_CAN_STBY]     = { GPIOD, GPIO_PIN_3,  true,  true  },
    [PAL_PIN_GNSS_RESET]   = { GPIOG, GPIO_PIN_2,  true,  false },
    [PAL_PIN_BLE_RESET]    = { GPIOG, GPIO_PIN_3,  true,  false },
    [PAL_PIN_BUZZER]       = { GPIOG, GPIO_PIN_6,  true,  false },
};

static void cfg(const pin_map_t *p, bool output)
{
    GPIO_InitTypeDef g = {
        .Pin = p->pin,
        .Mode = output ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(p->port, &g);
}

void pal_gpio_init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    for (int i = 0; i < PAL_PIN_COUNT; i++) {
        const pin_map_t *p = &k_pins[i];
        if (p->output)
            HAL_GPIO_WritePin(p->port, p->pin, p->init_level ? GPIO_PIN_SET : GPIO_PIN_RESET);
        cfg(p, p->output);
    }
}

void pal_gpio_write(pal_pin_t pin, bool level)
{
    HAL_GPIO_WritePin(k_pins[pin].port, k_pins[pin].pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool pal_gpio_read(pal_pin_t pin)
{
    return HAL_GPIO_ReadPin(k_pins[pin].port, k_pins[pin].pin) == GPIO_PIN_SET;
}

void pal_gpio_set_output(pal_pin_t pin, bool output)
{
    cfg(&k_pins[pin], output);
}
