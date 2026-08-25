/* Basic string operations
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

void _memcpy(char *d, char *s, int len)
{
	int i = 0;
	for (i = 0; i < len; i++)
		d[i] = s[i];
}

void _strcpy(char *d, char *s)
{
	int i = 0;
	while ((d[i] = s[i]) != '\0')
		i++;
}

void _memset(void *d, int v, int len)
{
	int i = 0;
	for (i = 0; i < len; i++)
		((char *)d)[i] = v;
}

int _strcmp(char *d, char *s)
{
	while (1) {
		if (*d != *s)
			return 1;
		if (*d == '\0' || *s == '\0')
			return 0;
		d++;
		s++;
	}
	return 1;
}

int _strnlen(const char *s, int count)
{
	const char *sc;

	for (sc = s; count-- && *sc != '\0'; ++sc)
		/* nothing */ ;
	return sc - s;
}
