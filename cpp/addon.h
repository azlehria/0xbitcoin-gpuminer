/*********************************************************************
 * NAN - Native Abstractions for Node.js
 *
 * Copyright (c) 2017 NAN contributors
 *
 * MIT License <https://github.com/nodejs/nan/blob/master/LICENSE.md>
 ********************************************************************/

#ifndef _ADDON_H_
#define _ADDON_H_

#include <nan.h>
#include <inttypes.h>

#include "hybridminer.h"
#include "miner_state.h"

namespace miner
{
  using namespace Nan;

  extern ::HybridMiner* hybridminer;

  //call C++ dtors:
  auto cleanup( void* p ) -> void;

  class Miner : public AsyncWorker
  {
  public:
    Miner( Callback *callback ) noexcept;

    ~Miner() = default;

    // This function runs in a thread spawned by NAN
    auto Execute() -> void;

  private:
    // Executed when the async work is complete
    // this function will be run inside the main event loop
    // so it is safe to use V8 again
    auto HandleOKCallback() -> void;
  };

  // Run an asynchronous function
  //  First and only parameter is a callback function
  //  receiving the solution when found
  NAN_METHOD( run );
  NAN_METHOD( stop );
  NAN_METHOD( getTarget );
  NAN_METHOD( getChallenge );
  NAN_METHOD( getPreviousChallenge );
  NAN_METHOD( getPoolAddress );
  NAN_METHOD( setPrefix );
  NAN_METHOD( getPrefix );
  NAN_METHOD( setAddress );
  NAN_METHOD( getAddress );
  NAN_METHOD( getCustomDiff );
  NAN_METHOD( setDiff );
  NAN_METHOD( getDiff );
  NAN_METHOD( getPoolUrl );
  NAN_METHOD( resetHashCounter );
  NAN_METHOD( incSolCount );
  NAN_METHOD( getSolution );
  NAN_METHOD( log );
  NAN_METHOD( getTokenName );
  NAN_METHOD( getSubmitStale );

  // Defines the functions our add-on will export
  NAN_MODULE_INIT( Init );
}

#endif // !_ADDON_H_
