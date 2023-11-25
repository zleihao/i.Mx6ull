## platform_match函数

函数路径：**./drivers/base/platform.c**

```c
static int platform_match(struct device *dev, struct device_driver *drv)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct platform_driver *pdrv = to_platform_driver(drv);

	/* When driver_override is set, only bind to the matching driver */
	if (pdev->driver_override)
		return !strcmp(pdev->driver_override, drv->name);

	/* Attempt an OF style match first */
    //设备树匹配
	if (of_driver_match_device(dev, drv))
		return 1;

	/* Then try ACPI style match */
	if (acpi_driver_match_device(dev, drv))
		return 1;

	/* Then try to match against the id table */
	if (pdrv->id_table)
		return platform_match_id(pdrv->id_table, pdev) != NULL;

	/* fall-back to driver name match */
	return (strcmp(pdev->name, drv->name) == 0);
}
```

- struct platform_device *pdev = to_platform_device(dev)

​	使用to_platform_device得到**struct device *dev**所在的结构体起始地址，也即结构体：struct platform_device

- struct platform_driver *pdrv = to_platform_driver(drv)

​	使用to_platform_driver得到**struct device_driver *drv**所在的结构体起始地址，也即结构体：struct platform_driver

紧接着下面是5种设备与驱动匹配方式，只需关注**if(of_driver_match_device(dev, drv))**、**if (pdrv->id_table)**、**return (strcmp(pdev->name, drv->name) == 0)**三种即可。

1. 无设备树

``` c
if (pdrv->id_table)
		return platform_match_id(pdrv->id_table, pdev) != NULL;
```

这种是没有使用设备树的匹配方法。

2. 名字匹配

``` c
return (strcmp(pdev->name, drv->name) == 0);
```

这种是以上匹配方式都不满足进行名字匹配。

3. **设备树匹配**

接下来详细看一下 **of_driver_match_device(dev, drv)** 函数内容。

该函数定义在**：./include/linux/of_device.h** 文件中。

``` c
/**
 * of_driver_match_device - Tell if a driver's of_match_table matches a device.
 * @drv: the device_driver structure to test
 * @dev: the device structure to match against
 */
static inline int of_driver_match_device(struct device *dev,
					 const struct device_driver *drv)
{
	return of_match_device(drv->of_match_table, dev) != NULL;
}
```

紧接着又调用函数：of_match_device，该函数定义在：**./drivers/of/device.c** 中。

``` c
/**
 * of_match_device - Tell if a struct device matches an of_device_id list
 * @ids: array of of device match structures to search in
 * @dev: the of device structure to match against
 *
 * Used by a driver to check whether an platform_device present in the
 * system is in its list of supported devices.
 */
const struct of_device_id *of_match_device(const struct of_device_id *matches,
					   const struct device *dev)
{
	if ((!matches) || (!dev->of_node))
		return NULL;
	return of_match_node(matches, dev->of_node);
}
```

紧接着又调用函数：of_match_node，通过该函数名大概能得知要进行**设备树**相关匹配，该函数定义在：**./drivers/of/base.c** 中。

``` c
/**
 * of_match_node - Tell if a device_node has a matching of_match structure
 *	@matches:	array of of device match structures to search in
 *	@node:		the of device structure to match against
 *
 *	Low level utility function used by device matching.
 */
const struct of_device_id *of_match_node(const struct of_device_id *matches,
					 const struct device_node *node)
{
	const struct of_device_id *match;
	unsigned long flags;
	
    //自旋锁，加锁
	raw_spin_lock_irqsave(&devtree_lock, flags);
	match = __of_match_node(matches, node);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return match;
}
```

紧接着又调用函数：__of_match_node，该函数定义在：**./drivers/of/base.c** 中。

``` c
static
const struct of_device_id *__of_match_node(const struct of_device_id *matches,
					   const struct device_node *node)
{
	const struct of_device_id *best_match = NULL;
	int score, best_score = 0;

	if (!matches)
		return NULL;

	for (; matches->name[0] || matches->type[0] || matches->compatible[0]; matches++) {
		score = __of_device_is_compatible(node, matches->compatible,
						  matches->type, matches->name);
		if (score > best_score) {
			best_match = matches;
			best_score = score;
		}
	}

	return best_match;
}
```

在该函数中依次遍历结构体 **struct of_device_id** 的成员：**name、type、compatible** 值与设备树中对应的

**name、type、compatible**属性进行匹配，函数 __of_device_is_compatible 定义在：**./drivers/of/base.c** 中。

``` c
static int __of_device_is_compatible(const struct device_node *device,
				     const char *compat, const char *type, const char *name)
{
	struct property *prop;
	const char *cp;
	int index = 0, score = 0;

	/* Compatible match has highest priority */
	if (compat && compat[0]) {
		prop = __of_find_property(device, "compatible", NULL);
		for (cp = of_prop_next_string(prop, NULL); cp;
		     cp = of_prop_next_string(prop, cp), index++) {
			if (of_compat_cmp(cp, compat, strlen(compat)) == 0) {
				score = INT_MAX/2 - (index << 2);
				break;
			}
		}
		if (!score)
			return 0;
	}

	/* Matching type is better than matching name */
	if (type && type[0]) {
		if (!device->type || of_node_cmp(type, device->type))
			return 0;
		score += 2;
	}

	/* Matching name is a bit better than not */
	if (name && name[0]) {
		if (!device->name || of_node_cmp(name, device->name))
			return 0;
		score++;
	}

	return score;
}
```

通过观察函数得知，匹配次序为：**compatible、type、name**，led的设备树信息如下：

``` dtd
gpio_led {
		compatible = "alientek,gpio_led";
		pinctrl-name = "defaule";
		pinctrl-0 = <&pinctrl_gpio_led>;
		led-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>; 
		status = "okay";
	};
```

可知在设备树中只有 **compatible** 一个属性，因此在编写led驱动时，在初始化结构体struct of_device_id时，只初始化成员：**compatible** 即可，将成员compatible初始化成与设备树对应的名字: **"alientek,gpio_led"**就可以实现**platform设备与platform驱动的匹配**。

**总结：**整体的函数调用关系为：

``` c
platform_match
    -> of_driver_match_device
    	-> of_match_device
    		-> of_match_node
    			-> __of_match_node
    				-> __of_device_is_compatible
    					-> ...
```



