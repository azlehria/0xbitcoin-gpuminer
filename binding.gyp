{
  'conditions': [
    [ 'OS=="win"', {'variables': {'obj': 'obj'}},
      {'variables': {'obj': 'o'}}]],

  "targets": [
    {
      "target_name": "hybridminer",
      "sources": [
        "cpp/hybridminer/addon.cc",
        "cpp/hybridminer/hybridminer.cpp",
        "cpp/hybridminer/cpusolver.cpp",
        "cpp/hybridminer/sha3.c",
        "cpp/hybridminer/cudasolver.cu"
      ],
      'cflags_cc+': [ '-march=native', '-O3', '-std=c++11' ],

      "include_dirs": ["<!(node -e \"require('nan')\")"],

      'rules': [{
        'extension': 'cu',
        'inputs': ['<(RULE_INPUT_PATH)'],
        'outputs':['<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).o'],
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
                   '/usr/local/cuda/lib64',
                   './cuda'],
        }],
        [ 'OS=="win"', {
          'conditions': [
            ['target_arch=="x64"',
             { 'variables': { 'arch': 'x64' }},
             { 'variables': { 'arch': 'Win32' }}
            ],
          ],
          'variables': {
            'cuda_root%': '$(CUDA_PATH)'
          },

          'libraries': [
            'cudart_static.lib',
            'cudasolver.o'
          ],

          'library_dirs': [
            '<(cuda_root)/lib/<(arch)',
            '<(module_root_dir)/build/Release/obj/hybridminer'
          ],

          "include_dirs": [
            "<(cuda_root)/include",
            'cpp/hybridminer'
          ]
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
              'WarningLevel': 3,
              'ExceptionHandling': 1,
              'DebugInformationFormat': 3,
              'AdditionalIncludeDirectories': [ '..\\cpp\\hybridminer' ]
            }
          }
        }
      },
    }
  ]
}
