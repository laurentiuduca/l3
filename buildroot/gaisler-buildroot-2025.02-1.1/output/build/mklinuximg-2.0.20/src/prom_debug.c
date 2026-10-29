/* Misc debug functions
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

#include <common.h>
#include <prom.h>

#ifdef CONFIG_DEBUG

static int lo_vsnprintf(char *buf, int size, const char *fmt, int *args)
{
	int len;
	int vac = 0;
	unsigned long long num;
	int i, j, n;
	char *str, *end;
	const char *s;
	int field_width;
	int precision;
	int filler;
	int l;
	char *b;

	str = buf;
	end = buf + size - 1;

	if (end < buf - 1) {
		/* end = ((void *) -1); */
		/* size = end - buf + 1; */
		*buf = 0;
		return 0;
	}

	for (; *fmt; ++fmt) {
		if (*fmt != '%') {
			if (str <= end)
				*str = *fmt;
			++str;
			continue;
		}

		/* get field width */
		field_width = 0;
		/* get the precision */
		precision = -1;
		filler = ' ';

		++fmt;

		/* default base */
		switch (*fmt) {
		case 's':
			s = (char *)args[vac++];
			if (!s)
				s = "<NULL>";

			len = _strnlen(s, precision);

			for (i = 0; i < len; ++i) {
				if (str <= end)
					*str = *s;
				++str;
				++s;
			}
			while (len < field_width--) {
				if (str <= end)
					*str = ' ';
				++str;
			}
			continue;
		case 'c':
			j = (char)args[vac++];
			if (str <= end) {
				if ((j >= 'a' && j <= 'z') ||
				    (j >= 'A' && j <= 'Z') ||
				    (j >= '0' && j <= '9') ||
				    j == '_')
					*str = j;
				else
					*str = '.';
			}
			str++;
			continue;

		case '%':
			if (str <= end)
				*str = '%';
			++str;
			continue;

		case 'p':
			break;
		case 'x':
			break;
		case 'd':
			break;

		default:
			if (str <= end)
				*str = '%';
			++str;
			if (*fmt) {
				if (str <= end)
					*str = *fmt;
				++str;
			} else {
				--fmt;
			}
			continue;
		}
		num = args[vac++];

		b = str;
		for (j = 0, i = 0; i < 8 && str <= end; i++) {
			n = ((unsigned long)(num & (0xf0000000UL >> (i * 4))))
				>> ((7 - i) * 4);
			if (n || j != 0) {
				j = 1;
				if (n >= 10)
					n += 'a' - 10;
				else
					n += '0';
				*str = n;
				++str;
			}
		}
		l = (str - b);
		if (l < field_width) {
			for (i = 0; i < (field_width - l) && str <= end; i++) {
				for (j = 0; j < l; j++)
					str[-j] = str[-(j + 1)];
				str[-l] = filler;
				str++;
			}
			j = 1;
		}

		if (j == 0 && str <= end) {
			*str = '0';
			++str;
		}
	}
	if (str <= end)
		*str = '\0';
	else if (size > 0)
		/* don't write out a null byte if the buf size is zero */
		*end = '\0';
	/* the trailing null byte doesn't count towards the total
	 * ++str;
	 */
	return str - buf;
}

int _printf(const char *fmt, int a0, int a1, int a2, int a3, int a4, int a5,
		int a6)
{
	char buf[256];
	int args[7];
	int len;

	args[0] = a0;
	args[1] = a1;
	args[2] = a2;
	args[3] = a3;
	args[4] = a4;
	args[5] = a5;
	args[6] = a6;

	/* Emit the output into the temporary buffer */
	len = lo_vsnprintf(buf, sizeof(buf) - 1, fmt, args);
	buf[len] = '\0';

	/* Print string onto UART */
	outstr(buf);

	return len;
}

#endif
