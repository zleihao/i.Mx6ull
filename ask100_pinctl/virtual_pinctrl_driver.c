#include "linux/kernel.h"
#include "linux/printk.h"
#include <asm-generic/errno-base.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/slab.h>
#include "core.h"

static struct pinctrl_dev *g_pinctrl_dev;

static unsigned long g_configs[4];

struct pinctrl_pin_desc const pins[] = {
    {0, "pin0", NULL},
    {1, "pin1", NULL},
    {2, "pin2", NULL},
    {3, "pin3", NULL},
};

struct virtual_func_desc {
    const char *func_name;
    const char **groups;
    int group_num;
};

static const char *func0_grps[] = {"pin0", "pin1", "pin2", "pin3"};
static const char *func1_grps[] = {"pin0", "pin1"};
static const char *func2_grps[] = {"pin2", "pin3"};

const struct virtual_func_desc g_func_desc[] = {
	{"gpio", func0_grps, 4},
	{"i2c",  func1_grps, 2},
	{"uart", func2_grps, 2},
};

static const struct of_device_id virtual_pinctrl_of_match[] = {
	{ .compatible = "100ask,virtual_pinctrl", },
	{ },
};

int virtual_get_groups_count(struct pinctrl_dev *pctldev)
{
	return pctldev->desc->npins;
}

static const char *virtual_get_group_name(struct pinctrl_dev *pctldev,
				       unsigned selector)
{
	return pctldev->desc->pins[selector].name;
}

static int virtual_get_group_pins(struct pinctrl_dev *pctldev, unsigned selector,
			       const unsigned **pins,
			       unsigned *npins)
{
	if (selector >= pctldev->desc->npins)
		return -EINVAL;

	*pins = &pctldev->desc->pins[selector].number;
	*npins = 1;

	return 0;
}

static void virtual_pin_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
		   unsigned offset)
{
	seq_printf(s, "%s", dev_name(pctldev->dev));
}

static int virtual_dt_node_to_map(struct pinctrl_dev *pctldev,
			struct device_node *np,
			struct pinctrl_map **map, unsigned *num_maps)
{
    int num_pins = 0, i;
    const char *pin;
    const char *function;
    unsigned int config;
    unsigned long *configs;
    struct pinctrl_map *new_map;
    
    /* 1. 确定pin个数/分配pinctrl_map */
    while (1) {
        if (of_property_read_string_index(np, "groups", num_pins, &pin) == 0) {
            num_pins++;
        } else {
            break;
        }
    }

    new_map = kmalloc(sizeof(struct pinctrl_map) * num_pins * 2, GFP_KERNEL);

    for (i = 0; i < num_pins; i++) {
        of_property_read_string_index(np, "groups", i, &pin);
        of_property_read_string_index(np, "functions", i, &function);

        of_property_read_u32_index(np, "configs", i, &config);

        configs = kmalloc(sizeof(*configs), GFP_KERNEL);
        
        new_map[i*2].type = PIN_MAP_TYPE_MUX_GROUP;
        new_map[i*2].data.mux.function = function;
        new_map[i*2].data.mux.group = pin;

        new_map[i*2+1].type = PIN_MAP_TYPE_CONFIGS_PIN;
        new_map[i*2+1].data.configs.group_or_pin = pin;
        new_map[i*2+1].data.configs.configs = configs;
        configs[0] = config;
        new_map[i*2+1].data.configs.num_configs = 1;
    }

    *map = new_map;
	*num_maps = num_pins * 2;

    return 0;
}

static void virtual_dt_free_map(struct pinctrl_dev *pctldev,
				struct pinctrl_map *map, unsigned num_maps)
{
    while (num_maps--) {
        if (map->type == PIN_MAP_TYPE_CONFIGS_PIN) {
            kfree(map->data.configs.configs);
        }

        kfree(map);
        map++;
    }
}

static const struct pinctrl_ops virtual_pctrl_ops = {
	.get_groups_count = virtual_get_groups_count,
	.get_group_name = virtual_get_group_name,
	.get_group_pins = virtual_get_group_pins,
	.pin_dbg_show = virtual_pin_dbg_show,
	.dt_node_to_map = virtual_dt_node_to_map,
	.dt_free_map = virtual_dt_free_map,

};

static int virtual_pmx_get_funcs_count(struct pinctrl_dev *pctldev)
{

	return ARRAY_SIZE(g_func_desc);
}

static const char *virtual_pmx_get_func_name(struct pinctrl_dev *pctldev,
					  unsigned selector)
{
	return g_func_desc[selector].func_name;
}

static int virtual_pmx_get_groups(struct pinctrl_dev *pctldev, unsigned selector,
			       const char * const **groups,
			       unsigned * const num_groups)
{
	*groups = g_func_desc[selector].groups;
	*num_groups = g_func_desc[selector].group_num;

	return 0;
}

static int virtual_pmx_set(struct pinctrl_dev *pctldev, unsigned selector,
		       unsigned group)
{
    printk("set %s as %s\r\n", pctldev->desc->pins[group].name, g_func_desc[selector].func_name);
    return 0;
}

static const struct pinmux_ops virtual_pmx_ops = {
	.get_functions_count = virtual_pmx_get_funcs_count,
	.get_function_name = virtual_pmx_get_func_name,
	.get_function_groups = virtual_pmx_get_groups,
	.set_mux = virtual_pmx_set,
};

static int virtual_pinconf_get(struct pinctrl_dev *pctldev,
			     unsigned pin_id, unsigned long *config)
{
    *config = g_configs[pin_id];
    return 0;
}

static int virtual_pinconf_set(struct pinctrl_dev *pctldev,
			     unsigned pin_id, unsigned long *configs,
			     unsigned num_configs)
{
    if (num_configs != 1) {
        return -EINVAL;
    }

    g_configs[pin_id] = *configs;

    printk("set %s as 0x%lx\r\n", pctldev->desc->pins[pin_id].name, *configs);

    return 0;
}

static void virtual_pinconf_dbg_show(struct pinctrl_dev *pctldev,
				   struct seq_file *s, unsigned pin_id)
{
	seq_printf(s, "0x%lx", g_configs[pin_id]);
}

static void virtual_pinconf_group_dbg_show(struct pinctrl_dev *pctldev,
				   struct seq_file *s, unsigned pin_id)
{
	seq_printf(s, "0x%lx", g_configs[pin_id]);
}

static const struct pinconf_ops virtual_pinconf_ops = {
	.pin_config_get = virtual_pinconf_get,
	.pin_config_set = virtual_pinconf_set,
	.pin_config_dbg_show = virtual_pinconf_dbg_show,
	.pin_config_group_dbg_show = virtual_pinconf_group_dbg_show,
};

int virtual_pinctrl_probe(struct platform_device *pdev)
{
    struct pinctrl_desc *pinctrl;
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    /* a. 分配pinctrl_desc */
    pinctrl = devm_kzalloc(&pdev->dev, sizeof(*pinctrl), GFP_KERNEL);
    if (!pinctrl) {
        return -ENOMEM;
    }
		
    /* b. 设置pinctrl_desc */
    pinctrl->owner = THIS_MODULE;
	pinctrl->name = dev_name(&pdev->dev);

    /* b.1 pins and group */
    pinctrl->pins = pins;
	pinctrl->npins = ARRAY_SIZE(pins);

    pinctrl->pctlops = &virtual_pctrl_ops;
    
    /* b.2 pins mux */
    pinctrl->pmxops = &virtual_pmx_ops;

    /* b.3 pins config */
    pinctrl->confops = &virtual_pinconf_ops;
	

    /* c.注册 pinctrl_desc */
    g_pinctrl_dev = pinctrl_register(pinctrl, &pdev->dev, NULL);

    return 0;
}

int virtual_pinctrl_remove(struct platform_device *dev)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}



static struct platform_driver virtual_pinctrl_driver = {
	.probe		= virtual_pinctrl_probe,
	.remove		= virtual_pinctrl_remove,
	.driver		= {
		.name	= "100ask_virtual_pinctrl",
		.of_match_table = of_match_ptr(virtual_pinctrl_of_match),
	}
};

//入口
static int __init virtual_pinctrl_init(void)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return platform_driver_register(&virtual_pinctrl_driver);
}

static void __exit virtual_pinctrl_exit(void)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    platform_driver_unregister(&virtual_pinctrl_driver);
}

module_init(virtual_pinctrl_init);
module_exit(virtual_pinctrl_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlh");