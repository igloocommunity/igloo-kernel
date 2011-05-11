#ifndef __REGULATOR_UX500_H
#define __REGULATOR_UX500_H

struct platform_device;

/**
 * struct u8500_regulator_info - u8500 regulator information
 * @dev: device pointer
 * @desc: regulator description
 * @rdev: regulator device pointer
 * @is_enabled: status of the regulator
 * @epod_id: id for EPOD (power domain)
 * @is_ramret: RAM retention switch for EPOD (power domain)
 * @operating_point: operating point (only for vape, to be removed)
 * @exclude_from_power_state: don't let this regulator prevent ApSLeep
 */
struct u8500_regulator_info {
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	bool is_enabled;
	u16 epod_id;
	bool is_ramret;
	bool exclude_from_power_state;
	unsigned int operating_point;
};

extern struct regulator_ops ux500_regulator_ops;
extern struct regulator_ops ux500_regulator_switch_ops;

int ux500_regulator_probe(struct platform_device *pdev,
			  struct u8500_regulator_info *info,
			  int num_regulators);

int ux500_regulator_remove(struct platform_device *pdev,
			   struct u8500_regulator_info *info,
			   int num_regulators);

#endif
