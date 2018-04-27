{
  'conditions': [
    [ 'OS=="win"',
      {'variables': {'obj': 'obj'}},
      {'variables': {'obj': 'o'}}
    ]
  ],

  "targets": [
    {
      "target_name": "hybridminer",
      "sources": [
        "cpp/hybridminer/addon.cc",
        "cpp/hybridminer/hybridminer.cpp",
        "cpp/hybridminer/cpusolver.cpp",
        "cpp/hybridminer/miner_state.cpp",
        "cpp/hybridminer/sha3.c",
        "cpp/hybridminer/cudasolver.cpp",
        "cpp/hybridminer/cuda_sha3.cu"
      ],
      'cflags_cc+': [ '-march=native', '-O3', '-std=c++11', '-Wall' ],

# Comment next line for test builds
      'defines': [ 'NDEBUG'],

      "include_dirs": ["<!(node -e \"require('nan')\")"],

      'rules': [{
        'extension': 'cu',
        'inputs': ['<(RULE_INPUT_PATH)'],
        'outputs':['<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).<(obj)'],
        'conditions': [
          [ 'OS=="win"',
            {'rule_name': 'cuda on windows',
             'message': "compile cuda file on windows",
             'process_outputs_as_sources': 0,
             'action': ['nvcc -c <(_inputs) -o <(_outputs)\
                        -cudart static -m64 -use_fast_math -O3',
                        '-gencode=arch=compute_70,code=\\\"sm_70,compute_70\\\"',
                        '-gencode=arch=compute_61,code=\\\"sm_61,compute_61\\\"',
                        '-gencode=arch=compute_52,code=\\\"sm_52,compute_52\\\"',
                        '-gencode=arch=compute_50,code=\\\"sm_50,compute_50\\\"',
                        '-gencode=arch=compute_35,code=\\\"sm_35,compute_35\\\"',
                        '-gencode=arch=compute_30,code=\\\"sm_30,compute_30\\\"']
            },
            {'rule_name': 'cuda on linux',
             'message': "compile cuda file on linux",
             'process_outputs_as_sources': 1,
             'action': ['nvcc','-std=c++11','-m64','-Xcompiler=\"-fpic\"',
                        '-c','<@(_inputs)','-o','<@(_outputs)',
                        '-gencode=arch=compute_70,code=\"sm_70,compute_70\"',
                        '-gencode=arch=compute_61,code=\"sm_61,compute_61\"',
                        '-gencode=arch=compute_52,code=\"sm_52,compute_52\"',
                        '-gencode=arch=compute_50,code=\"sm_50,compute_50\"',
                        '-gencode=arch=compute_35,code=\"sm_35,compute_35\"',
                        '-gencode=arch=compute_30,code=\"sm_30,compute_30\"']
            }
          ]
        ]
      }],

      'conditions': [
        [ 'OS=="mac"', {
          'libraries': ['-framework CUDA'],
          'include_dirs': ['/usr/local/include'],
          'library_dirs': ['/usr/local/lib'],
        }],
        [ 'OS=="linux"', {
          'libraries': ['-lcudart_static'],
          'include_dirs': ['/usr/local/include'],
          'library_dirs': ['/usr/local/lib',
                           '/usr/local/cuda/lib64'],
        }],
        [ 'OS=="win"', {
          'libraries': [ 'cudart_static.lib'],
          'library_dirs': [ '$(CUDA_PATH)/lib/<(target_arch)'],
          "include_dirs": [ "$(CUDA_PATH)/include"]
        }]
      ],
      'configurations': {
        'Release': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'SuppressStartupBanner': 'true',
# Next line controls CPU ISA optimizations: 0=default, 1=SSE, 2=SSE2, 3=AVX, 4=none, 5=AVX2
#              'EnableEnhancedInstructionSet': 5,
              'FavorSizeOrSpeed': 1,
              'InlineFunctionExpansion': 2,
              'MultiProcessorCompilation': 'true',
              'Optimization': 3,
              'RuntimeLibrary': 0,
              'WarningLevel': 4, # Wall is _completely batshit_ in MSVC
              'DisableSpecificWarnings': ['4100'], # formal argument not used
              'ExceptionHandling': 1,
              'DebugInformationFormat': 3
            }
          }
        }
      },
    }
  ]
}
