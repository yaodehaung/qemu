# SPDX-License-Identifier: GPL-2.0-or-later

syscall_nr_generators += {
  'q32': generator(sh,
                   arguments: [meson.project_source_root() / 'linux-user/riscv/syscallhdr.sh',
                               '@INPUT@', '@OUTPUT@', '@EXTRA_ARGS@'],
                   output: '@BASENAME@_nr.h')
}
