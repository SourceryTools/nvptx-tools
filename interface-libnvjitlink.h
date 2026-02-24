/* Interface to the NVIDIA/CUDA nvJitLink library.

   Copyright (C) 2026 BayLibre

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with nvptx-tools; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#ifndef INTERFACE_LIBNVJITLINK_H
#define INTERFACE_LIBNVJITLINK_H 1

#include <sstream>
#include <stddef.h>

extern bool interface_libnvjitlink_init (std::ostream &error_stream);

extern bool interface_libnvjitlink_version (std::ostream &error_stream, unsigned int *major, unsigned int *minor);

extern bool interface_libnvjitlink_verify (std::ostream &error_stream, const char *filename, size_t n_link_options, const char **link_options);

#endif /* INTERFACE_LIBNVJITLINK_H */
