# Tests for DotNameBot Application

test_sources = [
  'AssetManagerTest.cpp',
  'ConsoleLoggerTest.cpp',
  'FileReaderTest.cpp',
]

foreach test_source : test_sources
  test_name = test_source.split('.')[0]
  
  test_exe = executable(test_name,
    test_source,
    include_directories: [inc_dirs, src_inc_dirs],
    dependencies: [lib_dep, gtest_dep, gtest_main_dep],
  )
  
  test(test_name, test_exe)
endforeach

# Live RSS feed tests – require internet access; run with `meson test --suite live`
rss_live_exe = executable('RssFeedLiveTest',
  'RssFeedLiveTest.cpp',
  include_directories: [inc_dirs, src_inc_dirs],
  dependencies: [lib_dep, gtest_dep, gtest_main_dep],
)

test('RssFeedLiveTest', rss_live_exe,
  suite: 'live',
  timeout: 120,
)
