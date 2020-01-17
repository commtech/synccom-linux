/*
	Copyright (C) 2016 Commtech, Inc.

	This file is part of synccom-linux.

	synccom-linux is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	synccom-linux is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with synccom-linux.	If not, see <http://www.gnu.org/licenses/>.

*/

#include <linux/version.h>
#include "sysfs.h"
#include "utils.h" /* str_to_register_offset */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)

static ssize_t register_store(struct kobject *kobj,
							  struct kobj_attribute *attr, const char *buf,
							  size_t count,
							  unsigned bar_number)
{
	struct synccom_port *port = 0;
	int register_offset = 0;
	unsigned value = 0;
	char *end = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	register_offset = str_to_register_offset(attr->attr.name);
	value = (unsigned)simple_strtoul(buf, &end, 16);

	if (register_offset >= 0) {
		synccom_port_set_register(port, bar_number, register_offset, value, 1);
		return count;
	}

	return 0;
}

static ssize_t register_show(struct kobject *kobj, struct kobj_attribute *attr,
							 char *buf, unsigned bar_number)

{
	struct synccom_port *port = 0;
	int register_offset = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	register_offset = str_to_register_offset(attr->attr.name);

	if (register_offset >= 0) {
		return sprintf(buf, "%08x\n", synccom_port_get_register(port, bar_number,
					   (unsigned)register_offset, 1));
	}

	return 0;
}

static ssize_t bar0_register_store(struct kobject *kobj,
								   struct kobj_attribute *attr, const char *buf,
								   size_t count)
{
	return register_store(kobj, attr, buf, count, 0);
}

static ssize_t bar0_register_show(struct kobject *kobj,
								  struct kobj_attribute *attr, char *buf)
{
	return register_show(kobj, attr, buf, 0);
}

static ssize_t bar2_register_store(struct kobject *kobj,
								   struct kobj_attribute *attr, const char *buf,
								   size_t count)
{
	return register_store(kobj, attr, buf, count, 2);
}

static ssize_t bar2_register_show(struct kobject *kobj,
								  struct kobj_attribute *attr, char *buf)
{
	return register_show(kobj, attr, buf, 2);
}

static struct kobj_attribute fifot_attribute =
	__ATTR(fifot, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute cmdr_attribute =
	__ATTR(cmdr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute ccr0_attribute =
	__ATTR(ccr0, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute ccr1_attribute =
	__ATTR(ccr1, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute ccr2_attribute =
	__ATTR(ccr2, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute bgr_attribute =
	__ATTR(bgr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute ssr_attribute =
	__ATTR(ssr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute smr_attribute =
	__ATTR(smr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute tsr_attribute =
	__ATTR(tsr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute tmr_attribute =
	__ATTR(tmr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute rar_attribute =
	__ATTR(rar, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute ramr_attribute =
	__ATTR(ramr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute ppr_attribute =
	__ATTR(ppr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute tcr_attribute =
	__ATTR(tcr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute vstr_attribute =
	__ATTR(vstr, SYSFS_READ_ONLY_MODE, bar0_register_show, 0);

static struct kobj_attribute star_attribute =
	__ATTR(star, SYSFS_READ_ONLY_MODE, bar0_register_show, 0);

static struct kobj_attribute imr_attribute =
	__ATTR(imr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute dpllr_attribute =
	__ATTR(dpllr, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute fstel_attribute =
	__ATTR(fstel, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute fstew_attribute =
	__ATTR(fstew, SYSFS_READ_WRITE_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute fcr_attribute =
	__ATTR(fcr, SYSFS_READ_WRITE_MODE, bar2_register_show, bar2_register_store);

#ifdef DEBUG
static struct kobj_attribute bcfl_attribute =
	__ATTR(bcfl, SYSFS_READ_ONLY_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute fifobc_attribute =
	__ATTR(fifobc, SYSFS_READ_ONLY_MODE, bar0_register_show, bar0_register_store);

static struct kobj_attribute fifofc_attribute =
	__ATTR(fifofc, SYSFS_READ_ONLY_MODE, bar0_register_show, bar0_register_store);
#endif

static struct attribute *register_attrs[] = {
#ifdef DEBUG
	&bcfl_attribute.attr,
	&fifobc_attribute.attr,
	&fifofc_attribute.attr,
#endif
	&fifot_attribute.attr,
	&cmdr_attribute.attr,
	&ccr0_attribute.attr,
	&ccr1_attribute.attr,
	&ccr2_attribute.attr,
	&bgr_attribute.attr,
	&ssr_attribute.attr,
	&smr_attribute.attr,
	&tsr_attribute.attr,
	&tmr_attribute.attr,
	&rar_attribute.attr,
	&ramr_attribute.attr,
	&ppr_attribute.attr,
	&tcr_attribute.attr,
	&vstr_attribute.attr,
	&star_attribute.attr,
	&imr_attribute.attr,
	&dpllr_attribute.attr,
	&fstel_attribute.attr,
	&fstew_attribute.attr,
	&fcr_attribute.attr,
	NULL,
};

struct attribute_group port_registers_attr_group = {
	.name = "registers",
	.attrs = register_attrs,
};








static ssize_t purge_tx(struct kobject *kobj, struct kobj_attribute *attr,
							  const char *buf, size_t count)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	synccom_port_purge_tx(port);

	return count;
}

static ssize_t purge_rx(struct kobject *kobj, struct kobj_attribute *attr,
							  const char *buf, size_t count)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	synccom_port_purge_rx(port);

	return count;
}

static struct kobj_attribute purge_tx_attribute =
	__ATTR(purge_tx, SYSFS_WRITE_ONLY_MODE, 0, purge_tx);

static struct kobj_attribute purge_rx_attribute =
	__ATTR(purge_rx, SYSFS_WRITE_ONLY_MODE, 0, purge_rx);

static struct attribute *command_attrs[] = {
	&purge_tx_attribute.attr,
	&purge_rx_attribute.attr,
	NULL,
};

struct attribute_group port_commands_attr_group = {
	.name = "commands",
	.attrs = command_attrs,
};









static ssize_t output_memory(struct kobject *kobj, struct kobj_attribute *attr,
							 char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_output_memory_usage(port));
}

static ssize_t input_memory(struct kobject *kobj, struct kobj_attribute *attr,
								  char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_input_memory_usage(port));
}

static struct kobj_attribute output_memory_attribute =
	__ATTR(output_memory, SYSFS_READ_ONLY_MODE, output_memory, 0);

static struct kobj_attribute input_memory_attribute =
	__ATTR(input_memory, SYSFS_READ_ONLY_MODE, input_memory, 0);


static struct attribute *info_attrs[] = {
	&output_memory_attribute.attr,
	&input_memory_attribute.attr,
	NULL,
};

struct attribute_group port_info_attr_group = {
	.name = "info",
	.attrs = info_attrs,
};






static ssize_t tx_modifiers_store(struct kobject *kobj,
								  struct kobj_attribute *attr, const char *buf,
								  size_t count)
{
	struct synccom_port *port = 0;
	unsigned value = 0;
	char *end = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	value = (unsigned)simple_strtoul(buf, &end, 16);

	synccom_port_set_tx_modifiers(port, value);

	return count;
}

static ssize_t tx_modifiers_show(struct kobject *kobj,
								  struct kobj_attribute *attr, char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_tx_modifiers(port));
}

static ssize_t ignore_timeout_store(struct kobject *kobj,
									struct kobj_attribute *attr, const char *buf,
									size_t count)
{
	struct synccom_port *port = 0;
	unsigned value = 0;
	char *end = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	value = (unsigned)simple_strtoul(buf, &end, 16);

	synccom_port_set_ignore_timeout(port, value);

	return count;
}

static ssize_t ignore_timeout_show(struct kobject *kobj,
								   struct kobj_attribute *attr, char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_ignore_timeout(port));
}

static ssize_t rx_multiple_store(struct kobject *kobj,
									struct kobj_attribute *attr, const char *buf,
									size_t count)
{
	struct synccom_port *port = 0;
	unsigned value = 0;
	char *end = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	value = (unsigned)simple_strtoul(buf, &end, 16);

	synccom_port_set_rx_multiple(port, value);

	return count;
}

static ssize_t rx_multiple_show(struct kobject *kobj,
								   struct kobj_attribute *attr, char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_rx_multiple(port));
}

static ssize_t append_status_store(struct kobject *kobj,
								   struct kobj_attribute *attr, const char *buf,
								   size_t count)
{
	struct synccom_port *port = 0;
	unsigned value = 0;
	char *end = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	value = (unsigned)simple_strtoul(buf, &end, 16);

	synccom_port_set_append_status(port, value);

	return count;
}

static ssize_t append_status_show(struct kobject *kobj,
								  struct kobj_attribute *attr, char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_append_status(port));
}

static ssize_t append_timestamp_store(struct kobject *kobj,
								   struct kobj_attribute *attr, const char *buf,
								   size_t count)
{
	struct synccom_port *port = 0;
	unsigned value = 0;
	char *end = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	value = (unsigned)simple_strtoul(buf, &end, 16);

	synccom_port_set_append_timestamp(port, value);

	return count;
}

static ssize_t append_timestamp_show(struct kobject *kobj,
								  struct kobj_attribute *attr, char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_append_timestamp(port));
}

static ssize_t input_memory_cap_store(struct kobject *kobj,
								   struct kobj_attribute *attr, const char *buf,
								   size_t count)
{
	struct synccom_port *port = 0;
	struct synccom_memory_cap memory_cap;
	char *end = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	SYNCCOM_MEMORY_CAP_INIT(memory_cap);

	memory_cap.input = (int)simple_strtoul(buf, &end, 10);

	synccom_port_set_memory_cap(port, &memory_cap);

	return count;
}

static ssize_t input_memory_cap_show(struct kobject *kobj,
								  struct kobj_attribute *attr, char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_input_memory_cap(port));
}

static ssize_t output_memory_cap_store(struct kobject *kobj,
								   struct kobj_attribute *attr, const char *buf,
								   size_t count)
{
	struct synccom_port *port = 0;
	struct synccom_memory_cap memory_cap;
	char *end = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	SYNCCOM_MEMORY_CAP_INIT(memory_cap);

	memory_cap.output = (int)simple_strtoul(buf, &end, 10);

	synccom_port_set_memory_cap(port, &memory_cap);

	return count;
}

static ssize_t output_memory_cap_show(struct kobject *kobj,
								  struct kobj_attribute *attr, char *buf)
{
	struct synccom_port *port = 0;

	port = (struct synccom_port *)dev_get_drvdata((struct device *)kobj);

	return sprintf(buf, "%i\n", synccom_port_get_output_memory_cap(port));
}

static struct kobj_attribute append_status_attribute =
	__ATTR(append_status, SYSFS_READ_WRITE_MODE, append_status_show, append_status_store);

static struct kobj_attribute append_timestamp_attribute =
	__ATTR(append_timestamp, SYSFS_READ_WRITE_MODE, append_timestamp_show, append_timestamp_store);

static struct kobj_attribute input_memory_cap_attribute =
	__ATTR(input_memory_cap, SYSFS_READ_WRITE_MODE, input_memory_cap_show, input_memory_cap_store);

static struct kobj_attribute output_memory_cap_attribute =
	__ATTR(output_memory_cap, SYSFS_READ_WRITE_MODE, output_memory_cap_show, output_memory_cap_store);

static struct kobj_attribute ignore_timeout_attribute =
	__ATTR(ignore_timeout, SYSFS_READ_WRITE_MODE, ignore_timeout_show, ignore_timeout_store);

static struct kobj_attribute rx_multiple_attribute =
	__ATTR(rx_multiple, SYSFS_READ_WRITE_MODE, rx_multiple_show, rx_multiple_store);

static struct kobj_attribute tx_modifiers_attribute =
	__ATTR(tx_modifiers, SYSFS_READ_WRITE_MODE, tx_modifiers_show, tx_modifiers_store);

static struct attribute *settings_attrs[] = {
	&append_status_attribute.attr,
	&append_timestamp_attribute.attr,
	&input_memory_cap_attribute.attr,
	&output_memory_cap_attribute.attr,
	&ignore_timeout_attribute.attr,
	&rx_multiple_attribute.attr,
	&tx_modifiers_attribute.attr,
	NULL,
};

struct attribute_group port_settings_attr_group = {
	.name = "settings",
	.attrs = settings_attrs,
};






#ifdef DEBUG


static struct attribute *debug_attrs[] = {
	&rfs_count_attribute.attr,
	&rft_count_attribute.attr,
	&rfe_count_attribute.attr,
	&rfo_count_attribute.attr,
	&rdo_count_attribute.attr,
	&rfl_count_attribute.attr,

	&tin_count_attribute.attr,

	&dr_hi_count_attribute.attr,
	&dt_hi_count_attribute.attr,
	&dr_fe_count_attribute.attr,
	&dt_fe_count_attribute.attr,
	&dr_stop_count_attribute.attr,
	&dt_stop_count_attribute.attr,

	&tft_count_attribute.attr,
	&alls_count_attribute.attr,
	&tdu_count_attribute.attr,

	&ctss_count_attribute.attr,
	&dsrc_count_attribute.attr,
	&cdc_count_attribute.attr,
	&ctsa_count_attribute.attr,
	NULL,
};

struct attribute_group port_debug_attr_group = {
	.name = "debug",
	.attrs = debug_attrs,
};

#endif /* DEBUG */

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25) */

