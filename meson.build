project('linuxcnc-unit-test','c')

src_root = 'src'
tp_src_dir = join_paths(src_root, 'emc/tp')

inc_dir = include_directories([
  'unit_tests',
  src_root,
  tp_src_dir,
  'src/libnml/posemath',
  'src/emc/nml_intf',
  'src/emc/motion',
  'src/rtapi',
  ])

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)

add_global_arguments(['-DULAPI','-DUNIT_TEST'], language : 'c')

pm = static_library('posemath',
  'src/libnml/posemath/_posemath.c',
  include_directories : inc_dir)

# TODO implement subdir builds for tp, motion, etc

# KLUDGE just shove all the source files into the test build
test_blendmath=executable('test_blendmath',
  ['unit_tests/tp/test_blendmath.c',
    join_paths(tp_src_dir, 'blendmath.c'), 
    join_paths(tp_src_dir, 'spherical_arc.c'), 
    join_paths(src_root, 'emc/nml_intf/emcpose.c'), 
    join_paths(tp_src_dir, 'tc.c'), 
    ],
  dependencies : [m_dep],
  include_directories : inc_dir,
  link_with : pm
  )

test('blendmath test',test_blendmath)
