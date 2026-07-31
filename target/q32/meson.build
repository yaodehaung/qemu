# SPDX-License-Identifier: GPL-2.0-or-later

gen = decodetree.process('insns.decode')

q32_ss = ss.source_set()
q32_ss.add(gen)
q32_ss.add(files(
  'cpu.c',
  'disas.c',
  'exception_helper.c',
  'translate.c',
))

target_arch += {'q32': q32_ss}
