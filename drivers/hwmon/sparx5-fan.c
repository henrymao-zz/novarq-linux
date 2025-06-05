// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>

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

static int s5_fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s5_fan_data *priv;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	priv->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

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
