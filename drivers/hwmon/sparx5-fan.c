// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>

#define FAN_CFG				0
#define  FAN_CFG_DUTY_CYCLE		GENMASK(23, 16)
#define  FAN_CFG_INV_POL		BIT(3)
#define  FAN_CFG_GATE_ENA		BIT(2)
#define  FAN_CFG_PWM_OPEN_COL_ENA	BIT(1)
#define  FAN_CFG_PWM_STAT_CFG		BIT(0)
#define PWM_FREQ			4
#define  PWM_FREQ_CLK_CYCLES_10US	GENMASK(27, 16)
#define  PWM_FREQ_PWM_FREQ		GENMASK(15, 0)
#define FAN_CNT				8
#define  FAN_CNT_FAN_CNT		GENMASK(15, 0)

struct s5_fan_data {
	void __iomem *base;
	struct clk *clk;
};

static inline struct s5_fan_data *
s5_pwm_chip_to_data(struct pwm_chip *chip)
{
	return pwmchip_get_drvdata(chip);
}

static int s5_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			const struct pwm_state *state)
{
	struct s5_fan_data *priv = s5_pwm_chip_to_data(chip);
	u32 fan_cfg_val, pwm_freq_val;
	u64 pwm_frequency;

	pwm_freq_val = readl(priv->base + PWM_FREQ);
	pwm_frequency = clk_get_rate(priv->clk) / (256 * FIELD_GET(PWM_FREQ_PWM_FREQ, pwm_freq_val));

	fan_cfg_val = readl(priv->base + FAN_CFG);
	fan_cfg_val &= ~FAN_CFG_DUTY_CYCLE;
	fan_cfg_val |= FIELD_PREP(FAN_CFG_DUTY_CYCLE, (state->duty_cycle * 255) / state->period);
	fan_cfg_val &= ~FAN_CFG_INV_POL;
	fan_cfg_val |= FIELD_PREP(FAN_CFG_INV_POL, state->polarity);
	writel(fan_cfg_val, priv->base + FAN_CFG);

	dev_info(&chip->dev, "desired period: %llu\n", state->period);
	dev_info(&chip->dev, "desired duty_cycle: %llu\n", state->duty_cycle);
	dev_info(&chip->dev, "desired polarity: %d\n", state->polarity);

	return 0;
}

static int s5_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
			    struct pwm_state *state)
{
	struct s5_fan_data *priv = s5_pwm_chip_to_data(chip);
	u32 fan_cfg_val, pwm_freq_val;
	bool polarity;

	fan_cfg_val = readl(priv->base + FAN_CFG);
	polarity = FIELD_GET(FAN_CFG_INV_POL, fan_cfg_val);
	state->polarity = polarity ? PWM_POLARITY_INVERSED : PWM_POLARITY_NORMAL;
	state->enabled = FIELD_GET(FAN_CFG_DUTY_CYCLE, fan_cfg_val) ? true : false;

	pwm_freq_val = readl(priv->base + PWM_FREQ);
	// (System clock frequency)/(PWM frequency)/256
	state->period = NSEC_PER_SEC * 256 * FIELD_GET(PWM_FREQ_PWM_FREQ, pwm_freq_val) / clk_get_rate(priv->clk);

	state->duty_cycle = state->period / FIELD_GET(FAN_CFG_DUTY_CYCLE, fan_cfg_val);

	return 0;
}

static const struct pwm_ops s5_pwm_ops = {
	.apply = s5_pwm_apply,
	.get_state = s5_pwm_get_state,
};

static int s5_fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s5_fan_data *priv;
	struct pwm_chip *chip;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	priv->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

	chip = devm_pwmchip_alloc(dev, 1, sizeof(*priv));
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	pwmchip_set_drvdata(chip, priv);
	chip->ops = &s5_pwm_ops;

	ret = devm_pwmchip_add(dev, chip);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add PWM chip\n");

	return 0;
}

static const struct of_device_id s5_fan_match[] = {
	{ .compatible = "microchip,sparx5-fan" },
	{},
};
MODULE_DEVICE_TABLE(of, s5_fan_match);

static struct platform_driver s5_fan_driver = {
	.probe = s5_fan_probe,
	.driver = {
		.name = "sparx5-fan",
		.of_match_table = s5_fan_match,
	},
};

module_platform_driver(s5_fan_driver);

MODULE_AUTHOR("Robert Marko <robert.marko@sartura.hr>");
MODULE_DESCRIPTION("Sparx5 and LAN969x fan controller driver");
MODULE_LICENSE("GPL");
