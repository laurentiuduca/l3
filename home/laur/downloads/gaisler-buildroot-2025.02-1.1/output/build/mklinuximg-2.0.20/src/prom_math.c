/* Integer DIV and MUL operations, some LEON hardware does not have MUL/DIV
 * instructions so these functions are used instead
 *
 * Copyright (C) 2011 Frontgrade Gaisler AB
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

/* avoid udiv reference */
unsigned int int_div(unsigned int v, unsigned int d)
{
	unsigned int s = d, a = 1, r = 0, i;
	if (d == 0)
		return 0;
	for (i = 0; i < 32; i++) {
		if (s >= v)
			break;
		s = s << 1;
		a = a << 1;
	}
	while (s >= d) {
		while (s <= v) {
			v -= s;
			r += a;
		}
		s = s >> 1;
		a = a >> 1;
	}
	return r;
}

/* avoid umul reference */
unsigned int int_mul(unsigned int v, unsigned int m)
{
	unsigned int r = 0, i;
	for (i = 0; i < 32; i++) {
		if (m & 1)
			r += v;
		m = m >> 1;
		v = v << 1;
	}
	return r;
}
