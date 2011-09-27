#ifndef __ASM_ARCH_GPIO_H
#define __ASM_ARCH_GPIO_H

/*
 * 288 (#267 is the highest one actually hooked up) onchip GPIOs, plus enough
 * room for a couple of GPIO expanders.
 */
#define ARCH_NR_GPIOS	355
#define NOMADIK_NR_GPIO	288

#define MOP500_EGPIO(x)		(NOMADIK_NR_GPIO + (x))
#define MOP500_EGPIO_END	MOP500_EGPIO(24)
#define AB8500_GPIO_BASE	MOP500_EGPIO_END

#endif /* __ASM_ARCH_GPIO_H */
