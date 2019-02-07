project('linuxcnc-unit-test','c')

src_root = 'src'
tp_src_dir = join_paths(src_root, 'emc/tp')

inc_dir = include_directories([
  'unit_tests',
  'unit_tests/mock',
  src_root,
  join_paths(src_root, 'emc/kinematics'),
  tp_src_dir,
  'src/libnml/posemath',
  'src/emc/nml_intf',
  'src/emc/motion',
  'src/rtapi',
  'src/hal',
  ])

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)

add_global_arguments(['-DULAPI','-DUNIT_TEST','-DTP_PEDANTIC_DEBUG'], language : 'c')

pm = static_library('posemath',
  'src/libnml/posemath/_posemath.c',
  include_directories : inc_dir)

# TODO implement subdir builds for tp, motion, etc

# KLUDGE just shove all the source files into the test build
test_blendmath=executable('test_blendmath',
  ['unit_tests/tp/test_blendmath.c',
    join_paths(tp_src_dir, 'blendmath.c'), 
    join_paths(tp_src_dir, 'spherical_arc.c'), 
    join_paths(tp_src_dir, 'joint_util.c'),
    join_paths(src_root, 'emc/nml_intf/emcpose.c'), 
    join_paths(tp_src_dir, 'tc.c'), 
    ],
  dependencies : [m_dep],
  include_directories : inc_dir,
  link_with : pm
  )

test('blendmath test',test_blendmath)

test_joint_util=executable('test_joint_util',
  ['unit_tests/tp/test_joint_util.c',
    join_paths(tp_src_dir, 'joint_util.c'), 
    join_paths(src_root, 'emc/nml_intf/emcpose.c'), 
    ],
  dependencies : [m_dep],
  include_directories : inc_dir,
  link_with : pm
  )

test('joint_util test',test_joint_util)

test_spherical_arc=executable('test_spherical_arc',
  ['unit_tests/tp/test_spherical_arc.c',
    join_paths(tp_src_dir, 'spherical_arc.c'), 
    join_paths(src_root, 'emc/nml_intf/emcpose.c'), 
    ],
  dependencies : [m_dep],
  include_directories : inc_dir,
  link_with : pm
  )

test('spherical_arc test',test_spherical_arc)
