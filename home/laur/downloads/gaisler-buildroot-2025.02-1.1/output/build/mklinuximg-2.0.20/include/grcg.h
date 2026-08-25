/* Clock gating unit related definitions
 *
 * (C) Copyright 2024 Frontgrade Gaisler
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#ifndef __GRCG_H__
#define __GRCG_H__

/**
 * struct grcg_clkgate
 *
 * Contains a phandle to the producer that has a clock that can be indexed
 * by the hw_clk_gate_idx
 *
 * phandle         - The node reference to the producer
 * hw_clk_gate_idx - The index of a hw clock gate the clock gating core handles.
 */
struct grcg_clkgate {
	int phandle;
	int hw_clk_gate_idx;
};

/**
 * struct grcg_producer
 *
 * corenum              - The core subindex of the clock gating unit that this
 *                        producer represents
 * clks                 - An array of struct grcg_clkgate
 * num_clks             - The number elements in clks
 * clock_output_names   - The clock output names of this producer.
 *                        Is written to the "clock-output-names" property.
 * protected_clks       - An array of clks (hw gate indexes) which will be
 *                        ignored by the Linux driver.
 *                        Is optional and when enabled (not NULL) it is written
 *                        to the "protected-clocks" property.
 * num_protected_clks   - The number of elements in protected clks
 * clk_indices          - An array of clks (hw gate indexes) and should contain
 *                        the same number of items as in clock_output_names.
 *                        Is optional and when enabled (not NULL) it is written
 *                        to the "clock-indices" property.
 * num_clk_indices      - The number of elements in clk_indices
 * props_written        - The number of currently written producer properties
 * write_prop           - The function that is responsible for writing the
 *                        properties of this producer.
 *                        Input arguments:
 *                          buf           : The property buffer
 *                          producer      : The producer which the properties
 *                                          are written to.
 *                        Returns the number of bytes written to the property
 *                        buffer (buf) and should be called until it return 0.
 */
struct grcg_producer {
	unsigned short corenum;
	struct grcg_clkgate *clks;
	unsigned char num_clks;
	const char **clock_output_names;
	unsigned int *protected_clks;
	unsigned char num_protected_clks;
	unsigned int *clk_indices;
	unsigned char num_clk_indices;
	unsigned char props_written;
	int (*write_prop)(struct prop_std *buf, struct grcg_producer *self);
};

/**
 * struct grcg_consumer
 *
 * clk_id        - The id of the clock gate. It is expected that the id
 *                 starts from zero, is continous and can be used for
 *                 lookup of the corresponding struct grcg_clkgate from the
 *                 producer.
 * device        - The device (see ambapp_ids.h) that have cores that can
 *                 be clock gated.
 * corenum       - The subindex of a core that can be clock gated
 * producer      - The producer who owns the phandle+gate that the consumer
 *                 can lookup using clk_id when writing the "clocks" property.
 * props_written - The number of properties written to this consumer
 * write_prop    - The function that is responsible for writing the
 *                 properties of this consumer.
 *                 Input arguments:
 *                   buf       : The property buffer
 *                   consumer  : The consumer which the properties will
 *                               be written to.
 *                 Returns the number of bytes written to the property
 *                 buffer (buf) and should be called until it return 0.
 */
struct grcg_consumer {
	unsigned int clk_id;
	unsigned short device;
	unsigned char corenum;
	struct grcg_producer *producer;
	unsigned char props_written;
	int (*write_prop)(struct prop_std *buf, struct grcg_consumer *self);
};

/**
 * struct grcg
 *
 * is_enabled - Function that tells if a grcg is enabled or not.
 * consumers     - An array of clock gate consumers
 * num_consumers - The number of consumers
 * producers     - An array of clock gate producers
 * num_producers - The number of producers
 */
struct grcg {
	int (*is_enabled)(void);
	struct grcg_consumer *consumers;
	unsigned int num_consumers;
	struct grcg_producer **producers;
	unsigned int num_producers;
};

/**
 * Helper macro for defining a struct grcg_clkgate entry in an array
 * The phandle is default set to -1 which is reserved as illegal in Linux
 */
#define GRCG_CLKGATE_ENTRY(e, hw_clk_gate_idx)                                 \
	[e] = { -1, hw_clk_gate_idx }                                              \

/**
 * Helper macro for defining a struct grcg_consumer
 */
#define GRCG_CONSUMER(name, clk_id, device, corenum, producer, write_prop_fn)  \
	{                                                                          \
		clk_id, device, corenum, producer, 0, write_prop_fn                    \
	}

/* Get the size of a non-empty array */
#define GRCG_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

/* Helper macro for defining a struct grcg_producer which will expect that the
 * refered functions and variables are named in a certain way.
 */
#define GRCG_PRODUCER(name, corenum, write_prop_fn)                            \
	{                                                                          \
		corenum,                                                               \
		name##_producer_clks,                                                  \
		GRCG_ARRAY_SIZE(name##_producer_clks),                                 \
		name##_producer_clk_output_names,                                      \
		name##_producer_protected_clks,                                        \
		name##_producer_num_protected_clks,                                    \
		name##_producer_clk_indices,                                           \
		name##_producer_num_clk_indices,                                       \
		0,                                                                     \
		write_prop_fn                                                          \
	}

/* Helper macro for defining a struct grcg which will expect that the
 * refered functions and variables are named in a certain way.
 */
#define _GRCG_ENTRY(name, is_enabled_fn)                                       \
	{                                                                          \
		is_enabled_fn,                                                         \
		name##_consumers,                                                      \
		GRCG_ARRAY_SIZE(name##_consumers),                                     \
		name##_producers,                                                      \
		GRCG_ARRAY_SIZE(name##_producers),                                     \
	}
#define GRCG_ENTRY(name, is_enabled_fn) \
	_GRCG_ENTRY(name, is_enabled_fn)
#endif
